// Park work that hit a usage limit, and start parked work again when the limit is over.  No model credential here.
//
//   node queue.mjs hold       KIND=fix|triage|council NUMBER=<issue or PR> UNTIL=<epoch ms> [SHA=<issue fingerprint>]
//   node queue.mjs dispatch   (scheduled; starts the single most important parked item once nothing is waiting)
//
// See queue-lib.mjs for the marker and the priority order, and docs/AUTOMATION.md ("Usage limits").

import { REPO, api, gh } from './lib.mjs';
import { MAX_WAIT_MS } from './limits.mjs';
import { parseLevel } from './policy.mjs';
import { MAX_PARKS, QUEUE_LABEL, holdComment, parseMarker, pickNext } from './queue-lib.mjs';

const BOT = 'github-actions[bot]';
const mode = process.argv[2];

/** Newest marker comment from the automation on an issue or PR: { id, ...marker } or null. */
function markerOn(number) {
  const comments = api(`repos/${REPO}/issues/${number}/comments?per_page=100`, { paginate: true, allowFail: true }) ?? [];
  const mine = comments.filter((c) => c.user?.login === BOT && parseMarker(c.body));
  const last = mine[mine.length - 1];
  return last ? { id: last.id, ...parseMarker(last.body) } : null;
}

function hold() {
  const kind = process.env.KIND;
  const number = Number(process.env.NUMBER);
  if (!Number.isInteger(number) || number <= 0) throw new Error('NUMBER is required');
  const now = Date.now();
  const raw = Number(process.env.UNTIL);
  // Never park work for longer than the cap, whatever the caller passed.
  const until = Number.isFinite(raw) ? Math.min(Math.max(raw, now), now + MAX_WAIT_MS) : now + 3600_000;
  const sha = /^[0-9a-f]{64}$/.test(process.env.SHA ?? '') ? process.env.SHA : '';

  api(`repos/${REPO}/labels`, { method: 'POST', body: { name: QUEUE_LABEL, color: 'fbca04', description: 'Waiting for the AI models to be out of their usage limit' }, allowFail: true });
  api(`repos/${REPO}/issues/${number}/labels`, { method: 'POST', body: { labels: [QUEUE_LABEL] } });

  const existing = markerOn(number);
  const body = holdComment({ kind, until, sha, parks: (existing?.parks ?? 0) + 1 });
  if (existing) api(`repos/${REPO}/issues/comments/${existing.id}`, { method: 'PATCH', body: { body } });
  else api(`repos/${REPO}/issues/${number}/comments`, { method: 'POST', body: { body } });
  console.log(`parked ${kind} #${number} until ${new Date(until).toISOString()}`);
}

function dispatch() {
  const level = parseLevel(process.env.AUTOMATION_LEVEL);
  const fixOk = process.env.FIX_GATE_OK === 'true';
  const listed = api(`repos/${REPO}/issues?labels=${QUEUE_LABEL}&state=open&per_page=100`, { paginate: true }) ?? [];
  const items = [];
  for (const issue of listed) {
    const m = markerOn(issue.number);
    if (!m) { console.log(`#${issue.number}: labelled ${QUEUE_LABEL} but no marker from the automation; ignoring`); continue; }
    const kind = issue.pull_request ? 'council' : m.kind;
    if (kind !== m.kind) { console.log(`#${issue.number}: marker kind ${m.kind} does not match what it is; ignoring`); continue; }
    if (m.parks >= MAX_PARKS) {
      // Parked again and again: something other than a usage limit is wrong.  Ask a person instead of looping.
      console.log(`#${issue.number}: parked ${m.parks} times; asking for a human`);
      api(`repos/${REPO}/issues/${issue.number}/labels/${QUEUE_LABEL}`, { method: 'DELETE', allowFail: true });
      api(`repos/${REPO}/issues/${issue.number}/labels`, { method: 'POST', body: { labels: ['needs-human'] } });
      continue;
    }
    items.push({ number: issue.number, kind, until: m.until, sha: m.sha, labels: issue.labels.map((l) => l.name), createdAt: issue.created_at });
  }
  // What the automation level and the daily fix cap allow right now.
  const allowed = items.filter((i) => (i.kind === 'triage' ? level >= 1 : level >= 2) && (i.kind !== 'fix' || fixOk));
  console.log(`${items.length} parked, ${allowed.length} may start now (level ${level}, fix gate ${fixOk ? 'open' : 'closed'})`);

  const { next, holdUntil } = pickNext(allowed);
  if (holdUntil) { console.log(`holding: a limit lasts until ${new Date(holdUntil).toISOString()}`); return; }
  if (!next) { console.log('nothing to start'); return; }

  const workflow = { fix: 'agent-fix.yml', triage: 'triage.yml', council: 'council.yml' }[next.kind];
  const running = api(`repos/${REPO}/actions/workflows/${workflow}/runs?per_page=30`)?.workflow_runs ?? [];
  if (running.some((r) => ['queued', 'in_progress'].includes(r.status))) { console.log(`${workflow} is busy; next time`); return; }

  const args = ['workflow', 'run', workflow, '--repo', REPO, '--ref', 'main'];
  if (next.kind === 'fix') args.push('-f', `issue=${next.number}`, '-f', `body_sha=${next.sha}`);
  else if (next.kind === 'triage') args.push('-f', `issue=${next.number}`);
  else args.push('-f', `pr=${next.number}`);

  console.log(`starting ${next.kind} #${next.number} (${workflow})`);
  api(`repos/${REPO}/issues/${next.number}/labels/${QUEUE_LABEL}`, { method: 'DELETE', allowFail: true });
  try {
    gh(args);
  } catch (e) {
    api(`repos/${REPO}/issues/${next.number}/labels`, { method: 'POST', body: { labels: [QUEUE_LABEL] } });
    throw e;
  }
}

if (mode === 'hold') hold();
else if (mode === 'dispatch') dispatch();
else throw new Error(`unknown mode: ${mode}`);
