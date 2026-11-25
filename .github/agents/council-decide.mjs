// Council step 3: combine the verdicts, record the decision, and hand an approved PR to the merge policy.
// Runs WITHOUT model credentials.

import { spawnSync } from 'node:child_process';
import { readdirSync } from 'node:fs';

import { REPO, api, gh, readJson, sanitizeForComment } from './lib.mjs';
import { aggregate, isOnlyWaitingFor, validateVerdict } from './council-lib.mjs';

const input = readJson('council-input.json');
const { pr, sha } = input;

// The decision only counts for the exact commit that was reviewed.
const now = api(`repos/${REPO}/pulls/${pr}`);
if (now.head.sha !== sha) {
  console.log('the PR changed during review; discarding this decision');
  process.exit(0);
}

const verdicts = [];
const silent = [];
let limitedUntil = 0; // epoch ms; set when an expected reviewer was at a usage limit
for (const f of readdirSync('.').filter((n) => /^verdict-.+\.json$/.test(n))) {
  const v = readJson(f);
  // Only reviewers that were asked, for the commit that was reviewed, count.
  if (v.sha !== sha || !input.expected.includes(v.id)) continue;
  const id = sanitizeForComment(v.id, 60);
  if (v.limited) limitedUntil = Math.max(limitedUntil, Number(v.until) || 0);
  if (v.verdict) verdicts.push({ id, model: sanitizeForComment(v.model, 80), verdict: validateVerdict(v.verdict) });
  else silent.push(`${id} (${sanitizeForComment(v.error ?? 'no answer', 100)})`);
}

const result = aggregate(verdicts, input.classification.risk, input.expected);

// A reviewer at a usage limit is not a "no": if nobody who answered objects, postpone instead of rejecting, so the PR
// is reviewed again when the limit is over.  Any real objection still decides right away.
if (limitedUntil && isOnlyWaitingFor(verdicts, input.classification.risk)) {
  api(`repos/${REPO}/statuses/${sha}`, { method: 'POST', body: { state: 'pending', context: 'Council', description: 'waiting for a reviewer to be out of its usage limit' } });
  const r = spawnSync(process.execPath, ['.github/agents/queue.mjs', 'hold'], {
    env: { ...process.env, KIND: 'council', NUMBER: String(pr), UNTIL: String(limitedUntil) }, stdio: 'inherit',
  });
  console.log(`review postponed until ${new Date(limitedUntil).toISOString()} (exit ${r.status})`);
  process.exit(r.status ?? 1);
}
console.log(JSON.stringify({ approved: result.approved, risk: result.risk, reasons: result.reasons, participants: verdicts.map((v) => v.id) }));

// ---- record it: status on the reviewed commit, labels, a comment
api(`repos/${REPO}/statuses/${sha}`, {
  method: 'POST',
  body: {
    state: result.approved ? 'success' : 'failure',
    context: 'Council',
    description: result.approved ? `approved by ${verdicts.length} reviewers (${result.risk} risk)` : sanitizeForComment(result.reasons.join('; '), 130),
  },
});

const labels = [result.approved ? 'council-approved' : 'council-rejected', `risk:${result.risk}`];
for (const stale of ['council-approved', 'council-rejected', 'risk:low', 'risk:high', 'risk:blocked']) {
  if (!labels.includes(stale)) api(`repos/${REPO}/issues/${pr}/labels/${encodeURIComponent(stale)}`, { method: 'DELETE', allowFail: true });
}
api(`repos/${REPO}/issues/${pr}/labels`, { method: 'POST', body: { labels } });

const lines = [`## Council review of \`${sha.slice(0, 8)}\``, '', `**Decision: ${result.approved ? 'approved' : 'not approved'}** - risk class **${result.risk}**`, ''];
if (!result.approved) lines.push(`Why: ${result.reasons.join('; ')}`, '');
lines.push('| Reviewer | Verdict | Risk | Notes |', '|:--|:--|:--|:--|');
for (const { id, model, verdict: v } of verdicts) {
  const notes = [v.summary, ...v.concerns.map((c) => `${c.severity}: ${c.text}`)].filter(Boolean).join(' / ').replace(/\|/g, '/').slice(0, 500);
  lines.push(`| ${id} (${model}) | ${v.verdict.replace('_', ' ')} | ${v.risk} | ${notes} |`);
}
if (silent.length) lines.push('', `No answer from: ${silent.join(', ')}`);
lines.push('', '<sub>Reviewers are language models; their text above is advice and was sanitised. Approval also needs CI and the merge policy.</sub>');
api(`repos/${REPO}/issues/${pr}/comments`, { method: 'POST', body: { body: lines.join('\n') } });

// ---- approved: take it out of draft and let the merge policy look at it
if (result.approved) {
  if (now.draft) gh(['pr', 'ready', String(pr), '--repo', REPO]);
  gh(['workflow', 'run', 'automerge.yml', '--repo', REPO]);
} else if (input.issueNumber) {
  api(`repos/${REPO}/issues/${input.issueNumber}/labels`, { method: 'POST', body: { labels: ['needs-human'] } });
}
