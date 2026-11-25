// Usage limits are not failures.  When every model a stage may use is at a usage limit, a rate limit or is overloaded,
// the work is put back in the queue (queue.mjs) and retried later, highest priority first; it is never counted as a
// failed attempt and never marked "needs a human".
//
// Pure functions, no I/O.

export class LimitError extends Error {
  /** @param {string} message  @param {number} until  epoch milliseconds before which retrying is pointless */
  constructor(message, until) {
    super(message);
    this.name = 'LimitError';
    this.until = until;
  }
}

// What providers say when they refuse because of a limit: Claude Code ("usage limit reached|<epoch>", "5-hour limit",
// "weekly limit"), OpenRouter and most APIs (HTTP 429 / 529 / 402, "rate limit", "overloaded", "quota", "credits").
// Deliberately narrow: a deterministic failure (a prompt that is too long, a stack trace with a line number such as
// file.js:429) must NOT be mistaken for a limit, or it would be parked and retried instead of reported.  Status codes count
// only next to the words that introduce them, and token or context limits are not usage limits.
const LIMIT_RE = new RegExp([
  'usage limit', 'rate[ _-]?limit', 'too many requests', '(?:http|status|code|returned|response)\\D{0,12}\\b(?:429|529|402)\\b',
  '\\b(?:429|529)\\s+(?:too many|overloaded)', 'http/\\d(?:\\.\\d)?\\s+(?:429|529|402)\\b', 'overloaded', 'quota', '(?:5-hour|weekly|daily|monthly|session) limit', 'out of (?:credits|usage)',
  'insufficient (?:credits|funds|balance)', 'credit balance', 'resource[ _]exhausted',
].join('|'), 'i');

export const MIN_WAIT_MS = 10 * 60_000;
export const DEFAULT_WAIT_MS = 60 * 60_000;
export const MAX_WAIT_MS = 6 * 3600_000;

export function isLimitMessage(message) {
  return LIMIT_RE.test(String(message ?? ''));
}

/**
 * When may it be retried?  Reads what the message says ("...|1764100000" epoch seconds, an ISO time, or "in 20 minutes"),
 * otherwise waits an hour.  Always between 10 minutes and 6 hours from `now`: a wrong or hostile value cannot park work
 * forever, and a limit is never hammered.
 */
export function limitUntil(message, now = Date.now()) {
  const text = String(message ?? '');
  let when = null;
  const epoch = /\|\s*(\d{10})(?!\d)/.exec(text) ?? /\bresets?(?: at)?\s+(\d{10})(?!\d)/i.exec(text);
  if (epoch) when = Number(epoch[1]) * 1000;
  const iso = /\b(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}(?::\d{2})?(?:\.\d+)?Z)\b/.exec(text);
  if (when === null && iso) when = Date.parse(iso[1]);
  const rel = /\bin\s+(\d+)\s*(second|minute|min|hour|hr|h|m|s)s?\b/i.exec(text);
  if (when === null && rel) {
    const unit = rel[2].toLowerCase();
    const per = unit.startsWith('h') ? 3600_000 : unit.startsWith('m') ? 60_000 : 1000;
    when = now + Number(rel[1]) * per;
  }
  if (when === null || !Number.isFinite(when)) when = now + DEFAULT_WAIT_MS;
  return Math.min(Math.max(when, now + MIN_WAIT_MS), now + MAX_WAIT_MS);
}

/** The same error as a LimitError if it is a limit, otherwise null. */
export function asLimitError(e, now = Date.now()) {
  if (e instanceof LimitError) return e;
  const message = e instanceof Error ? e.message : String(e);
  return isLimitMessage(message) ? new LimitError(message.slice(0, 300), limitUntil(message, now)) : null;
}
