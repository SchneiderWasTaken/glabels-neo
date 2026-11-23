// Council step 1: collect what the reviewers will see, from GitHub, as data.  No model credential here.
// Nothing from the PR branch is executed; only its diff text is fetched.

import { REPO, api, gh, writeJson } from './lib.mjs';
import { parseDiff } from './diff-lib.mjs';
import { classifyChange } from './policy.mjs';
import { reviewerConfig } from './council-lib.mjs';
import { appendFileSync } from 'node:fs';

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
if (p.user.type !== 'Bot') problems.push('not opened by the automation');
if (problems.length) {
  console.log(`council not applicable: ${problems.join('; ')}`);
  appendFileSync(process.env.GITHUB_OUTPUT, 'ok=false\n');
  process.exit(0);
}

const diff = gh(['pr', 'diff', String(pr), '--repo', REPO]);
const { files, addedLines } = parseDiff(diff);
const classification = classifyChange(files, addedLines);

const issueNumber = Number(/Fixes #(\d+)/.exec(p.body ?? '')?.[1]);
const issue = Number.isInteger(issueNumber) ? api(`repos/${REPO}/issues/${issueNumber}`) : { title: '', body: '' };

writeJson('council-input.json', {
  pr,
  sha: p.head.sha,
  title: p.title,
  summary: p.body ?? '',
  issueNumber: Number.isInteger(issueNumber) ? issueNumber : null,
  issueTitle: issue.title,
  issueBody: issue.body ?? '',
  diff,
  files,
  classification,
});

// Matrix of reviewers that can run (external ones need their secret to exist; the workflow tells us which).
const available = new Set((process.env.AVAILABLE_SECRETS || '').split(',').filter(Boolean));
const reviewers = reviewerConfig().filter((r) => !r.needs || available.has(r.needs));
console.log(`PR #${pr} @ ${p.head.sha.slice(0, 8)}: ${files.length} files, deterministic risk ${classification.risk}; reviewers: ${reviewers.map((r) => r.id).join(', ')}`);
appendFileSync(process.env.GITHUB_OUTPUT, `ok=true\nreviewers=${JSON.stringify(reviewers)}\n`);
