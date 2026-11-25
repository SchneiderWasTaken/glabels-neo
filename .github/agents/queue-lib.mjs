// The work queue for usage limits: pure functions.  See queue.mjs for the part that talks to GitHub.
//
// When every model is at a limit, an item (an issue to fix, an issue to triage, a PR to review) is parked with the label
// `agent-queued` and ONE comment from the automation that carries a hidden marker saying what it is and when to retry.
// A scheduled dispatcher (agent-dispatch.yml) starts the single most important item once the wait is over.
//
// The marker is only believed when it was written by github-actions[bot]; the label alone does nothing.

export const QUEUE_LABEL = 'agent-queued';
export const MARKER_RE = /<!-- agent-queue kind=(fix|triage|council) until=(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)(?: sha=([0-9a-f]{64}))?(?: n=(\d{1,3}))? -->/;

/** How many times one item may be parked before a person is asked to look (a stuck "limit" must not loop forever). */
export const MAX_PARKS = 12;

/** The hidden marker for a comment.  `until` is epoch milliseconds. */
export function makeMarker({ kind, until, sha = '', parks = 1 }) {
  if (!['fix', 'triage', 'council'].includes(kind)) throw new Error(`unknown queue kind: ${kind}`);
  const iso = new Date(until).toISOString().replace(/\.\d{3}Z$/, 'Z');
  if (sha && !/^[0-9a-f]{64}$/.test(sha)) throw new Error('sha must be a 64-character hex string');
  return `<!-- agent-queue kind=${kind} until=${iso}${sha ? ` sha=${sha}` : ''} n=${Math.min(Math.max(Math.trunc(parks) || 1, 1), 999)} -->`;
}

/** { kind, until (epoch ms), sha, parks } from a comment body, or null. */
export function parseMarker(body) {
  const m = MARKER_RE.exec(String(body ?? ''));
  if (!m) return null;
  const until = Date.parse(m[2]);
  return Number.isFinite(until) ? { kind: m[1], until, sha: m[3] ?? '', parks: Number(m[4] ?? 1) } : null;
}

/**
 * Importance of a queued item; lower goes first.
 *   0  fixes for problems that block a user (they stop someone working)
 *   1  council reviews (almost finished work: a review frees a PR to merge)
 *   2  triage (cheap, and it decides what everything else is)
 *   3  fixes a maintainer asked for with `agent-fix`
 *   4  fixes triage marked `agent-ready`
 *   5  `auto-fix` issues (small changes a maintainer approved)
 *   6  anything else
 */
export function priorityOf({ kind, labels = [] }) {
  const has = (l) => labels.includes(l);
  if (kind === 'council') return 1;
  if (kind === 'triage') return 2;
  if (has('blocks-user')) return 0;
  if (has('agent-fix')) return 3;
  if (has('agent-ready')) return 4;
  if (has('auto-fix')) return 5;
  return 6;
}

/**
 * Which item to start now, if any.
 *   items: [{ number, kind, until, sha, labels, createdAt }]
 * Returns { next, holdUntil }.  A limit applies to every model at once, so ONE item still waiting means all wait.
 * Otherwise the most important item goes first; ties go to the oldest.
 */
export function pickNext(items, now = Date.now()) {
  const waiting = items.filter((i) => i.until > now).map((i) => i.until);
  if (waiting.length) return { next: null, holdUntil: Math.max(...waiting) };
  const sorted = [...items].sort((a, b) =>
    priorityOf(a) - priorityOf(b) || Date.parse(a.createdAt ?? 0) - Date.parse(b.createdAt ?? 0) || a.number - b.number);
  return { next: sorted[0] ?? null, holdUntil: null };
}

/** Comment text for a parked item (the marker goes first; everything else is for people). */
export function holdComment({ kind, until, sha, parks = 1 }) {
  const what = { fix: 'The automatic fix', triage: 'Triage', council: 'The review' }[kind];
  const at = new Date(until).toISOString().replace(/:\d{2}\.\d{3}Z$/, ' UTC').replace('T', ' ');
  return [
    makeMarker({ kind, until, sha, parks }),
    `${what} is waiting: the AI models the automation uses have reached a usage limit. It was not counted as a failure and nothing needs doing. It will start again by itself after about ${at}, most important work first.`,
  ].join('\n');
}
