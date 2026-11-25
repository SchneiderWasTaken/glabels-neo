// The work queue for usage limits: pure functions.  See queue.mjs for the part that talks to GitHub.
//
// When every model is at a limit, an item (an issue to fix, an issue to triage, a PR to review) is parked with the label
// `agent-queued` and ONE comment from the automation that carries a hidden marker saying what it is and when to retry.
// A scheduled dispatcher (agent-dispatch.yml) starts the single most important item once the wait is over, and then turns the
// marker into a "consumed" one, so a label added later by anyone cannot replay old work.
//
// The marker is only believed when it was written by github-actions[bot] AND the label was last applied by it; the label
// alone does nothing.

export const QUEUE_LABEL = 'agent-queued';
export const MARKER_RE = /<!-- agent-queue kind=(fix|triage|council) until=(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)(?: sha=([0-9a-f]{64}))?(?: n=(\d{1,3}))? -->/;
export const CONSUMED_RE = /<!-- agent-queue-consumed kind=(fix|triage|council) until=(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z) n=(\d{1,3}) -->/;

/** How many times one item may be parked in one episode before a person is asked to look (a stuck "limit" must not loop). */
export const MAX_PARKS = 12;
/** Parks that are further apart than this are separate episodes: the count starts again. */
export const EPISODE_MS = 24 * 3600_000;

const iso = (ms) => new Date(ms).toISOString().replace(/\.\d{3}Z$/, 'Z');
const clampParks = (n) => Math.min(Math.max(Math.trunc(n) || 1, 1), 999);

/** The hidden marker for a comment.  `until` is epoch milliseconds. */
export function makeMarker({ kind, until, sha = '', parks = 1 }) {
  if (!['fix', 'triage', 'council'].includes(kind)) throw new Error(`unknown queue kind: ${kind}`);
  if (sha && !/^[0-9a-f]{64}$/.test(sha)) throw new Error('sha must be a 64-character hex string');
  return `<!-- agent-queue kind=${kind} until=${iso(until)}${sha ? ` sha=${sha}` : ''} n=${clampParks(parks)} -->`;
}

/** A marker that has been used: it keeps the count for the episode but can no longer start anything. */
export function makeConsumedMarker({ kind, until, parks = 1 }) {
  if (!['fix', 'triage', 'council'].includes(kind)) throw new Error(`unknown queue kind: ${kind}`);
  return `<!-- agent-queue-consumed kind=${kind} until=${iso(until)} n=${clampParks(parks)} -->`;
}

/** { kind, until (epoch ms), sha, parks } from a comment body, or null (consumed markers are not live markers). */
export function parseMarker(body) {
  const m = MARKER_RE.exec(String(body ?? ''));
  if (!m) return null;
  const until = Date.parse(m[2]);
  return Number.isFinite(until) ? { kind: m[1], until, sha: m[3] ?? '', parks: Number(m[4] ?? 1) } : null;
}

/** { kind, until, parks } of a consumed marker, or null. */
export function parseConsumed(body) {
  const m = CONSUMED_RE.exec(String(body ?? ''));
  const until = m ? Date.parse(m[2]) : NaN;
  return Number.isFinite(until) ? { kind: m[1], until, parks: Number(m[3]) } : null;
}

/**
 * How many times is this item parked now?  `previous` is its newest marker (live or consumed).  It continues the count when
 * it is the same kind of work and recent; a different kind of work, or an earlier episode, starts again at 1.
 */
export function nextParks(previous, kind, now = Date.now()) {
  if (!previous || previous.kind !== kind) return 1;
  return now - previous.until < EPISODE_MS ? previous.parks + 1 : 1;
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
 * The items that may start now, most important first (oldest first within a tier).
 *   items: [{ number, kind, until, sha, labels, createdAt }]
 * Which limit holds which work:
 *   - a parked fix or triage means EVERY model was limited, so nothing can start until it is over;
 *   - a parked council review means a reviewer was limited, usually Claude, while triage and fixes can still run on the
 *     other models: it holds only other council reviews (which need the same reviewer).
 */
export function startable(items, now = Date.now()) {
  const waiting = items.filter((i) => i.until > now);
  const holdAll = Math.max(0, ...waiting.filter((i) => i.kind !== 'council').map((i) => i.until));
  const holdCouncil = Math.max(holdAll, ...waiting.filter((i) => i.kind === 'council').map((i) => i.until));
  return items
    .filter((i) => i.until <= now && (i.kind === 'council' ? holdCouncil : holdAll) <= now)
    .sort((a, b) => priorityOf(a) - priorityOf(b) || Date.parse(a.createdAt ?? 0) - Date.parse(b.createdAt ?? 0) || a.number - b.number);
}

/** Which item to start now, if any.  Returns { next, holdUntil } (holdUntil: when to look again, if only waiting). */
export function pickNext(items, now = Date.now()) {
  const [next] = startable(items, now);
  if (next) return { next, holdUntil: null };
  const waiting = items.filter((i) => i.until > now).map((i) => i.until);
  return { next: null, holdUntil: waiting.length ? Math.max(...waiting) : null };
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
