// Stage 1b: apply a validated triage result.  Runs without any model credential.

import { spawnSync } from 'node:child_process';

import { REPO, api, gh, readJson } from './lib.mjs';
import { parseLevel } from './policy.mjs';
import { agentReady, buildComment, labelsFor, validateTriage } from './triage-lib.mjs';

const data = readJson('triage-result.json');
if (data.skip) {
  console.log(`skipped: ${data.skip}`);
  process.exit(0);
}

if (data.limited) {
  // Not a failure: park the issue (it keeps `needs-triage`); agent-dispatch.yml triages it when the limit is over.
  const number = Number(data.number);
  const until = Number(data.until);
  if (!Number.isInteger(number) || number <= 0 || !Number.isFinite(until)) throw new Error('bad limited triage result');
  const r = spawnSync(process.execPath, ['.github/agents/queue.mjs', 'hold'], {
    env: { ...process.env, KIND: 'triage', NUMBER: String(number), UNTIL: String(until) }, stdio: 'inherit',
  });
  process.exit(r.status ?? 1);
}

const { number, fromApp } = data;
const issueNow = api(`repos/${REPO}/issues/${number}`);
if (issueNow.labels.some((l) => l.name === 'triaged')) {
  console.log('already triaged; not applying twice');
  process.exit(0);
}

// Re-validate: this job trusts nothing from the artifact beyond its shape.
const known = (api(`repos/${REPO}/issues?state=all&per_page=100`) ?? []).map((i) => i.number);
const t = validateTriage(data.triage, known);
const level = parseLevel(process.env.AUTOMATION_LEVEL);

const labels = labelsFor(t);
const ready = agentReady(t, level);
if (ready) labels.push('agent-ready');

api(`repos/${REPO}/issues/${number}/labels`, { method: 'POST', body: { labels } });
if (issueNow.labels.some((l) => l.name === 'needs-triage')) {
  api(`repos/${REPO}/issues/${number}/labels/needs-triage`, { method: 'DELETE', allowFail: true });
}
api(`repos/${REPO}/issues/${number}/comments`, { method: 'POST', body: { body: buildComment(t, { fromApp }) } });
console.log(`#${number}: labels ${labels.join(', ')}`);

if (ready) {
  console.log('handing to the fix agent');
  gh(['workflow', 'run', 'agent-fix.yml', '--repo', REPO, '-f', `issue=${number}`, '-f', `body_sha=${data.bodySha ?? ''}`]);
}
