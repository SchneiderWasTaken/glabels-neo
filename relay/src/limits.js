// Rate limiting and single-use challenges.
//
// The relay may run as a stateless function (DigitalOcean Functions), which has no storage of its own, so the
// production backend ("github") uses the repository as its state: every report the relay files carries a hidden marker
// with the reporter's hash and the challenge's nonce, and a new report is accepted only if the recent reports in the
// issue tracker leave room for it.  That list is strongly consistent, so a challenge cannot be replayed once its issue
// exists (only a burst of truly simultaneous requests can slip through, which the global cap still bounds).
//
// The in-memory backend is for local development and tests.

import { listRecentReports } from './github.js';

const HOUR = 3600_000;
const DAY = 86_400_000;
const MARKER_RE = /<!-- reporter:([0-9a-f]{8,64}) nonce:([A-Za-z0-9_-]{8,64}) -->/;

const memory = [];  // { at, reporter, nonce } of accepted reports

export function resetMemoryLimits() {
  memory.length = 0;
}

export async function sha256Hex(text) {
  const d = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text));
  return [...new Uint8Array(d)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

function limitsOf(env) {
  const num = (v, d) => (Number.isFinite(Number(v)) && Number(v) > 0 ? Number(v) : d);
  return {
    perHour: num(env.LIMIT_PER_HOUR, 3),
    perDay: num(env.LIMIT_PER_DAY, 8),
    global: num(env.LIMIT_GLOBAL_PER_DAY, 100),
  };
}

/** Reports filed in the last day, oldest first, without those the relay itself rejected afterwards. */
async function githubHistory(env, now, fetchFn) {
  const recent = await listRecentReports(env, now - DAY, fetchFn);
  return recent
    .filter((r) => !r.labels?.includes('relay-rejected'))
    .map((r) => {
      const m = MARKER_RE.exec(r.body ?? '');
      return { number: r.number, at: Date.parse(r.created_at), reporter: m ? m[1] : '', nonce: m ? m[2] : '' };
    })
    .filter((h) => Number.isFinite(h.at))
    .sort((a, b) => (a.number ?? 0) - (b.number ?? 0));
}

/**
 * Optimistic filing needs a second look: GitHub's issue list can lag a second or two, so a burst (or a replayed
 * challenge) can pass the first check several times.  A moment after filing, each report looks at who came before it
 * and, if it was a duplicate or over a limit, withdraws itself.  Earlier reports always win, so the outcome is the same
 * on every invocation.  Returns { ok: true } or { ok: false, why }.
 */
export async function reconcile(env, { number, nonce, reporter }, now = Date.now(), fetchFn = fetch) {
  const lim = limitsOf(env);
  const history = await githubHistory(env, now, fetchFn);
  const before = history.filter((h) => (h.number ?? 0) < number);
  const result = decide(before, { nonce, reporter }, now, lim);
  return result.ok ? { ok: true } : { ok: false, why: result.kind === 'nonce' ? 'this report was already submitted' : 'too many reports from this sender or overall' };
}

/** Decide from a list of earlier reports [{ at, reporter, nonce }]. */
function decide(history, { nonce, reporter }, now, lim) {
  const inDay = history.filter((h) => now - h.at < DAY);
  if (inDay.some((h) => h.nonce === nonce)) return { ok: false, kind: 'nonce' };
  if (inDay.length >= lim.global) return { ok: false, kind: 'limit', scope: 'global', retryAfter: 3600 };
  const mine = inDay.filter((h) => h.reporter === reporter);
  if (mine.filter((h) => now - h.at < HOUR).length >= lim.perHour) {
    return { ok: false, kind: 'limit', scope: 'reporter', retryAfter: 3600 };
  }
  if (mine.length >= lim.perDay) return { ok: false, kind: 'limit', scope: 'reporter', retryAfter: 6 * 3600 };
  return { ok: true };
}

/**
 * Claim a report.  Returns { ok: true } | { ok: false, kind: 'nonce' } | { ok: false, kind: 'limit', scope, retryAfter }.
 * Throws when the backend cannot be reached (callers must fail closed).
 */
export async function claimReport(env, { nonce, reporter }, now = Date.now(), fetchFn = fetch) {
  const lim = limitsOf(env);

  if (env.LIMITS_BACKEND === 'github') {
    return decide(await githubHistory(env, now, fetchFn), { nonce, reporter }, now, lim);
  }

  const result = decide(memory, { nonce, reporter }, now, lim);
  if (result.ok) memory.push({ at: now, reporter, nonce });
  return result;
}
