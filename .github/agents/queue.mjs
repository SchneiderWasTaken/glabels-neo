// Park work that hit a usage limit, and start parked work again when the limit is over.  No model credential here.
//
//   node queue.mjs hold       KIND=fix|triage|council NUMBER=<issue or PR> UNTIL=<epoch ms> [SHA=<issue fingerprint>]
//   node queue.mjs dispatch   (scheduled; starts the single most important parked item once nothing blocks it)
//
// See queue-lib.mjs for the markers, the priority order and which limit holds which work, and docs/AUTOMATION.md.
//
// Trust: an item counts only if it carries the label AND the label was last applied by the automation AND a live marker
// comment written by the automation says what it is.  Anyone with triage rights can add the label, so the label alone proves
// nothing; and starting an item turns its marker into a "consumed" one, so a label added later cannot replay it.

import { REPO, api, gh, issueHash } from './lib.mjs';
import { MAX_WAIT_MS } from './limits.mjs';
import { parseLevel } from './policy.mjs';
import {
  MAX_PARKS, QUEUE_LABEL, holdComment, makeConsumedMarker, nextParks, parseConsumed, parseMarker, startable,
} from './queue-lib.mjs';

const BOT = 'github-actions[bot]';
const WORKFLOWS = ['agent-fix.yml', 'triage.yml', 'council.yml'];
const mode = process.argv[2];

/** The newest marker comment (live or consumed) from the automation on an issue or PR, or null. */
function markerOn(number) {
  const comments = api(`repos/${REPO}/issues/${number}/comments?per_page=100`, { paginate: true, allowFail: true }) ?? [];
  const mine = comments.filter((c) => c.user?.login === BOT);
  for (let i = mine.length - 1; i >= 0; i--) {
    const live = parseMarker(mine[i].body);
    if (live) return { id: mine[i].id, consumed: false, ...live };
    const used = parseConsumed(mine[i].body);
    if (used) return { id: mine[i].id, consumed: true, sha: '', ...used };
  }
  return null;
}

/** Was the queue label last applied by the automation itself? */
function labelledByAutomation(number) {
  const events = api(`repos/${REPO}/issues/${number}/events?per_page=100`, { paginate: true, allowFail: true }) ?? [];
  const mine = events.filter((e) => e.event === 'labeled' && e.label?.name === QUEUE_LABEL);
  return mine.length > 0 && mine[mine.length - 1].actor?.login === BOT;
}

const removeLabel = (number) => api(`repos/${REPO}/issues/${number}/labels/${QUEUE_LABEL}`, { method: 'DELETE', allowFail: true });
const addLabel = (number) => api(`repos/${REPO}/issues/${number}/labels`, { method: 'POST', body: { labels: [QUEUE_LABEL] } });
const say = (number, body) => api(`repos/${REPO}/issues/${number}/comments`, { method: 'POST', body: { body } });

/** Turn a live marker into a consumed one (keeps the park count), and say so. */
function consume(item, note) {
  const body = [makeConsumedMarker({ kind: item.kind, until: item.until, parks: item.parks }), note].join('\n');
  api(`repos/${REPO}/issues/comments/${item.commentId}`, { method: 'PATCH', body: { body } });
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
  const previous = markerOn(number);
  const parks = nextParks(previous, kind, now);
  const body = holdComment({ kind, until, sha, parks });
  // The comment first: a label without a marker is ignored, so a failure half way never leaves something half-parked.
  if (previous) api(`repos/${REPO}/issues/comments/${previous.id}`, { method: 'PATCH', body: { body } });
  else say(number, body);
  addLabel(number);
  console.log(`parked ${kind} #${number} until ${new Date(until).toISOString()} (park ${parks})`);
}

/** Why parked work should not be started after all, or null. */
function notWanted(item) {
  const issue = api(`repos/${REPO}/issues/${item.number}`, { allowFail: true });
  if (!issue) return { why: 'it can no longer be read' };
  if (issue.state !== 'open') return { why: `it is ${issue.state}` };
  if (item.kind === 'council') return issue.pull_request ? null : { why: 'it is not a pull request' };
  if (issue.pull_request) return { why: 'it is a pull request' };
  if (item.kind === 'triage') return issue.labels.some((l) => l.name === 'triaged') ? { why: 'it was triaged meanwhile' } : null;
  if (item.sha && issueHash(issue.title, issue.body) !== item.sha) {
    return { why: 'the issue text was edited after it was approved for a fix', human: true };
  }
  const open = api(`repos/${REPO}/pulls?state=open&per_page=100`, { paginate: true }) ?? [];
  if (open.some((p) => p.head.ref.startsWith(`agent/issue-${item.number}-`))) return { why: 'an agent pull request is already open for it' };
  return null;
}

function dispatch() {
  const level = parseLevel(process.env.AUTOMATION_LEVEL);
  const fixOk = process.env.FIX_GATE_OK === 'true';
  const listed = api(`repos/${REPO}/issues?labels=${QUEUE_LABEL}&state=open&per_page=100`, { paginate: true }) ?? [];
  const items = [];
  for (const issue of listed) {
    const n = issue.number;
    const m = markerOn(n);
    if (!m || m.consumed) { console.log(`#${n}: labelled ${QUEUE_LABEL} but there is no live marker from the automation; ignoring`); continue; }
    if (!labelledByAutomation(n)) { console.log(`#${n}: the label was not applied by the automation; ignoring`); continue; }
    const kind = issue.pull_request ? 'council' : m.kind;
    if (kind !== m.kind) { console.log(`#${n}: marker kind ${m.kind} does not match what it is; ignoring`); continue; }
    const item = { number: n, kind, until: m.until, sha: m.sha, parks: m.parks, commentId: m.id, labels: issue.labels.map((l) => l.name), createdAt: issue.created_at };
    if (m.parks >= MAX_PARKS) {
      // Parked again and again: something other than a usage limit is wrong.  Ask a person instead of looping.
      console.log(`#${n}: parked ${m.parks} times; asking for a human`);
      consume(item, 'It was parked many times and never ran. A person needs to look at this one.');
      api(`repos/${REPO}/issues/${n}/labels`, { method: 'POST', body: { labels: ['needs-human'] } });
      removeLabel(n);
      continue;
    }
    items.push(item);
  }
  // What the automation level and the daily fix cap allow right now.
  const allowed = items.filter((i) => (i.kind === 'triage' ? level >= 1 : level >= 2) && (i.kind !== 'fix' || fixOk));
  const ready = startable(allowed);
  console.log(`${items.length} parked, ${allowed.length} allowed (level ${level}, fix gate ${fixOk ? 'open' : 'closed'}), ${ready.length} startable now`);
  if (!ready.length) return;

  // One at a time: a probe that has not parked itself again yet must not be joined by another.
  for (const wf of WORKFLOWS) {
    const runs = api(`repos/${REPO}/actions/workflows/${wf}/runs?per_page=30`)?.workflow_runs ?? [];
    if (runs.some((r) => ['queued', 'in_progress'].includes(r.status))) { console.log(`${wf} is busy; next time`); return; }
  }

  for (const next of ready) {
    const reason = notWanted(next);
    if (reason) {
      console.log(`${next.kind} #${next.number}: dropped (${reason.why})`);
      consume(next, `Not restarted automatically: ${reason.why}.`);
      removeLabel(next.number);
      if (reason.human) api(`repos/${REPO}/issues/${next.number}/labels`, { method: 'POST', body: { labels: ['needs-human'] } });
      continue;
    }
    const workflow = { fix: 'agent-fix.yml', triage: 'triage.yml', council: 'council.yml' }[next.kind];
    const args = ['workflow', 'run', workflow, '--repo', REPO, '--ref', 'main'];
    if (next.kind === 'fix') args.push('-f', `issue=${next.number}`, '-f', `body_sha=${next.sha}`);
    else if (next.kind === 'triage') args.push('-f', `issue=${next.number}`);
    else args.push('-f', `pr=${next.number}`);

    console.log(`starting ${next.kind} #${next.number} (${workflow})`);
    consume(next, 'Restarted automatically.');
    removeLabel(next.number);
    try {
      gh(args);
    } catch (e) {
      // Could not start it: put it back exactly as it was.
      api(`repos/${REPO}/issues/comments/${next.commentId}`, { method: 'PATCH', body: { body: holdComment(next) } });
      addLabel(next.number);
      throw e;
    }
    return;
  }
}

if (mode === 'hold') hold();
else if (mode === 'dispatch') dispatch();
else throw new Error(`unknown mode: ${mode}`);
