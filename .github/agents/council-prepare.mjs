// Council step 1: collect what the reviewers will see, from GitHub, as data.  No model credential here.
// Nothing from the PR branch is executed; only its diff text is fetched.

import { appendFileSync } from 'node:fs';

import { REPO, api, gh, writeJson } from './lib.mjs';
import { autoFixApproval } from './autofix.mjs';
import { reviewerConfig } from './council-lib.mjs';
import { STAGES } from './model-policy.mjs';
import { parseDiff } from './diff-lib.mjs';
import { classifyChange } from './policy.mjs';

const pr = Number(process.env.PR);
if (!Number.isInteger(pr) || pr <= 0) throw new Error('PR is required');

const p = api(`repos/${REPO}/pulls/${pr}`);
const labels = p.labels.map((l) => l.name);

const problems = [];
if (p.state !== 'open') problems.push('PR is not open');
if (p.base.ref !== 'main') problems.push('base is not main');
if (p.head.repo?.full_name !== REPO) problems.push('head is not in this repository');
if (!p.head.ref.startsWith('agent/issue-')) problems.push('not an agent branch');
if (!labels.includes('agent-pr')) problems.push('missing agent-pr label');
if (p.user.login !== 'github-actions[bot]') problems.push('not opened by the automation');
if (problems.length) {
  console.log(`council not applicable: ${problems.join('; ')}`);
  appendFileSync(process.env.GITHUB_OUTPUT, 'ok=false\n');
  process.exit(0);
}

const sha = p.head.sha;
const diff = gh(['pr', 'diff', String(pr), '--repo', REPO]);
// The diff and the commit it belongs to must be the same snapshot.
if (api(`repos/${REPO}/pulls/${pr}`).head.sha !== sha) throw new Error('the PR changed while it was being read; run the council again');

const { files, addedLines } = parseDiff(diff);

const issueNumber = Number(/Fixes #(\d+)/.exec(p.body ?? '')?.[1]);
// `auto-fix`: asked from GitHub (a person with write access applied it), never taken from the PR text.
const autoFix = Number.isInteger(issueNumber) && autoFixApproval(issueNumber).approved;
const classification = classifyChange(files, addedLines, { diffChars: diff.length, autoFix });
const issue = Number.isInteger(issueNumber) ? api(`repos/${REPO}/issues/${issueNumber}`) : { title: '', body: '' };

// Reviewers that can run (external ones need their secret to exist; the workflow tells us which).
const available = new Set((process.env.AVAILABLE_SECRETS || '').split(',').filter(Boolean));
const reviewers = reviewerConfig({ autoFix }).filter((r) => !r.needs || available.has(r.needs));

writeJson('council-input.json', {
  pr,
  sha,
  title: p.title,
  summary: p.body ?? '',
  issueNumber: Number.isInteger(issueNumber) ? issueNumber : null,
  issueTitle: issue.title,
  issueBody: issue.body ?? '',
  diff,
  files,
  classification,
  expected: reviewers.map((r) => r.id), // every one of these must answer for an approval
  autoFix,
  required: autoFix ? [...STAGES['council-auto']] : [], // must take part even when their secret is missing
});

console.log(`PR #${pr} @ ${sha.slice(0, 8)}: ${files.length} files, deterministic risk ${classification.risk}; reviewers: ${reviewers.map((r) => r.id).join(', ')}`);
appendFileSync(process.env.GITHUB_OUTPUT, `ok=true\nreviewers=${JSON.stringify(reviewers)}\n`);
