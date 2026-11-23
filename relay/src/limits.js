// Rate limiting.  Counters live in a Cloudflare KV namespace (binding RATE) when available; otherwise
// in memory, which is only meaningful for local development and tests.
//
// KV is eventually consistent and has no atomic increment, so the limits are deliberately approximate:
// they stop floods and casual abuse, they are not an accounting system.

const memory = new Map();

async function get(env, key) {
  if (env.RATE) return Number((await env.RATE.get(key)) ?? 0);
  const e = memory.get(key);
  return e && e.exp > Date.now() ? e.n : 0;
}

async function put(env, key, n, ttlSeconds) {
  if (env.RATE) return env.RATE.put(key, String(n), { expirationTtl: Math.max(60, ttlSeconds) });
  memory.set(key, { n, exp: Date.now() + ttlSeconds * 1000 });
}

export function resetMemoryLimits() {
  memory.clear();
}

export async function sha256Hex(text) {
  const d = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text));
  return [...new Uint8Array(d)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

/** Remember a proof-of-work nonce; false when it was already used. */
export async function claimNonce(env, nonce, ttlSeconds) {
  const key = `pow:${nonce}`;
  if ((await get(env, key)) > 0) return false;
  await put(env, key, 1, ttlSeconds);
  return true;
}

/**
 * Check the per-reporter hourly/daily limits and the global daily limit, and count this report.
 * Returns { ok: true } or { ok: false, scope, retryAfter }.
 */
export async function checkAndCount(env, reporterId, now = Date.now()) {
  const num = (v, d) => (Number.isFinite(Number(v)) && Number(v) > 0 ? Number(v) : d);
  const perHour = num(env.LIMIT_PER_HOUR, 3);
  const perDay = num(env.LIMIT_PER_DAY, 8);
  const global = num(env.LIMIT_GLOBAL_PER_DAY, 100);

  const hour = Math.floor(now / 3600000);
  const day = Math.floor(now / 86400000);
  const kh = `h:${reporterId}:${hour}`;
  const kd = `d:${reporterId}:${day}`;
  const kg = `g:${day}`;

  const [h, d, g] = await Promise.all([get(env, kh), get(env, kd), get(env, kg)]);
  if (g >= global) return { ok: false, scope: 'global', retryAfter: 3600 };
  if (h >= perHour) return { ok: false, scope: 'reporter', retryAfter: 3600 - Math.floor((now % 3600000) / 1000) };
  if (d >= perDay) return { ok: false, scope: 'reporter', retryAfter: 86400 - Math.floor((now % 86400000) / 1000) };

  await Promise.all([put(env, kh, h + 1, 3700), put(env, kd, d + 1, 86500), put(env, kg, g + 1, 86500)]);
  return { ok: true };
}

/**
 * The single entry point used by the request handler: claim the challenge (single use) and count the report against
 * the limits, atomically when the LIMITER Durable Object is bound, approximately (KV or memory) otherwise.
 * Returns { ok: true } | { ok: false, kind: 'nonce' } | { ok: false, kind: 'limit', scope, retryAfter }.
 */
export async function claimReport(env, { nonce, nonceExpires, reporter }, now = Date.now()) {
  const num = (v, d) => (Number.isFinite(Number(v)) && Number(v) > 0 ? Number(v) : d);
  if (env.LIMITER) {
    const stub = env.LIMITER.get(env.LIMITER.idFromName('global'));
    const res = await stub.fetch('https://limiter.internal/claim', {
      method: 'POST',
      body: JSON.stringify({
        nonce, nonceExpires, reporter, now,
        perHour: num(env.LIMIT_PER_HOUR, 3), perDay: num(env.LIMIT_PER_DAY, 8), global: num(env.LIMIT_GLOBAL_PER_DAY, 100),
      }),
    });
    return res.json();
  }
  if (!(await claimNonce(env, nonce, 900))) return { ok: false, kind: 'nonce' };
  const r = await checkAndCount(env, reporter, now);
  return r.ok ? { ok: true } : { ok: false, kind: 'limit', scope: r.scope, retryAfter: r.retryAfter };
}
