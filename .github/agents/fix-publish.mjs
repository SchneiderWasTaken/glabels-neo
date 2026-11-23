// Stage 2b: turn the fix agent's result into a draft pull request.  Runs WITHOUT any model credential.
//
// The patch is data: it is re-analysed here (never trust the artifact), must apply cleanly to the current
// default branch, and is only ever pushed to a fresh `agent/issue-N-*` branch as a draft PR.  The council,
// CI and the merge policy decide what happens next; this script never merges.

import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';

import { REPO, api, assertNoSecrets, gh, readJson, sanitizeForComment } from './lib.mjs';
import { addedLinesOf, classifyChange } from './policy.mjs';

const data = readJson('fix-result.json');
const { number, outcome } = data;
const title = sanitizeForComment(data.title, 80).replace(/\s+/g, ' ');

function git(...args) {
  const r = spawnSync('git', args, { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  if (r.status !== 0) throw new Error(`git ${args[0]} failed: ${(r.stderr || '').slice(0, 300)}`);
  return r.stdout;
}

function comment(body) {
  api(`repos/${REPO}/issues/${number}/comments`, { method: 'POST', body: { body } });
}

function needsHuman(note) {
  api(`repos/${REPO}/issues/${number}/labels`, { method: 'POST', body: { labels: ['needs-human'] } });
  api(`repos/${REPO}/issues/${number}/labels/agent-ready`, { method: 'DELETE', allowFail: true });
  if (note) comment(note);
}

if (outcome.status !== 'ok') {
  const why =
    outcome.status === 'cannot-fix'
      ? `The fix agent decided it could not fix this safely: ${outcome.reason || 'no reason given'}`
      : outcome.status === 'blocked'
        ? `The fix agent's change was rejected by the safety rules (${(outcome.reasons ?? []).join('; ')}).`
        : `The fix agent could not produce a passing change${outcome.reason ? ` (${outcome.reason.slice(0, 300)})` : ''}.`;
  needsHuman(`I tried to prepare a fix automatically and could not.\n\n${why}\n\nA maintainer needs to look at this one.`);
  console.log(`no PR: ${outcome.status}`);
  process.exit(0);
}

if (!existsSync('fix.patch')) throw new Error('fix.patch is missing');
const patch = readFileSync('fix.patch', 'utf8');
assertNoSecrets(patch, []);

// Re-derive the change from the patch itself.
git('apply', '--check', 'fix.patch');
const files = git('apply', '--numstat', 'fix.patch')
  .trim()
  .split('\n')
  .filter(Boolean)
  .map((line) => {
    const [add, del, ...rest] = line.split('\t');
    return { path: rest.join('\t'), status: 'modified', additions: add === '-' ? 0 : Number(add), deletions: del === '-' ? 0 : Number(del), binary: add === '-' };
  });
const summaryLines = git('apply', '--summary', 'fix.patch').split('\n');
for (const f of files) {
  if (summaryLines.some((l) => l.includes(`create mode`) && l.endsWith(` ${f.path}`) && !l.includes('100644'))) f.mode = 'exec';
  if (summaryLines.some((l) => l.includes('mode change') && l.endsWith(` ${f.path}`))) f.mode = 'changed';
  if (summaryLines.some((l) => l.includes('delete mode') && l.endsWith(` ${f.path}`))) f.status = 'removed';
  if (summaryLines.some((l) => l.includes('create mode') && l.endsWith(` ${f.path}`))) f.status = 'added';
}
const classification = classifyChange(files, addedLinesOf(patch));
if (classification.risk === 'blocked') {
  needsHuman(`The fix agent's change was rejected by the safety rules (${classification.reasons.slice(0, 5).join('; ')}).`);
  console.log('no PR: blocked on re-check');
  process.exit(0);
}

const branch = `agent/issue-${number}-${Date.now().toString(36)}`;
git('config', 'user.name', 'gLabels Neo Bot');
git('config', 'user.email', '41898282+github-actions[bot]@users.noreply.github.com');
git('checkout', '-b', branch);
git('apply', 'fix.patch');
git('add', '-A', '--', ...files.map((f) => f.path));

const summary = sanitizeForComment(outcome.summary, 700);
const message = [
  `fix: ${title.slice(0, 68)} (#${number})`,
  '',
  summary,
  '',
  `Fixes #${number}`,
  '',
  'Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>',
].join('\n');
writeFileSync('commit-message.txt', message);
git('commit', '-F', 'commit-message.txt');
git('push', 'origin', branch);

const body = [
  `Fixes #${number}`,
  '',
  '## What changed (written by the fix agent)',
  summary || '_(no summary)_',
  '',
  '## Automated assessment',
  `- Risk class: **${classification.risk}**${classification.reasons.length ? ` (${classification.reasons.slice(0, 5).join('; ')})` : ''}`,
  `- Files: ${files.map((f) => `\`${f.path}\``).join(', ')}`,
  `- Tests added or extended: ${outcome.testsAdded ? 'yes' : 'no'}`,
  '- The project was built and the unit tests were run before this PR was proposed.',
  '',
  '> Opened automatically from an issue, which is untrusted input. This PR stays a draft until the review council',
  '> approves it and CI passes. Changes outside the allowed source directories, or that touch networking, process',
  '> execution or the build, are never merged automatically.',
  '',
  '🤖 Generated with [Claude Code](https://claude.com/claude-code)',
].join('\n');
writeFileSync('pr-body.md', body);

const prUrl = gh([
  'pr', 'create', '--repo', REPO, '--draft', '--base', 'main', '--head', branch,
  '--title', `fix: ${title.slice(0, 68)} (#${number})`, '--body-file', 'pr-body.md',
]).trim();
const prNumber = Number(prUrl.split('/').pop());
console.log(`opened ${prUrl}`);

api(`repos/${REPO}/issues/${prNumber}/labels`, { method: 'POST', body: { labels: ['agent-pr', `risk:${classification.risk}`] } });
comment(`I opened a draft pull request for this: #${prNumber}. It will be reviewed automatically before anything is merged.`);

// Pushes made with the workflow token do not trigger pull_request workflows, so start them explicitly.
for (const wf of ['ci.yml', 'lint.yml']) gh(['workflow', 'run', wf, '--repo', REPO, '--ref', branch]);
gh(['workflow', 'run', 'council.yml', '--repo', REPO, '-f', `pr=${prNumber}`]);
