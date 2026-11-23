// Stateless proof-of-work challenges.
//
// A reporter has no account, so each report must cost the sender a little CPU time.  The relay hands
// out a signed challenge; the client searches for a counter such that
//     SHA-256( challenge + ":" + counter )
// starts with `bits` zero bits, then returns challenge and counter with the report.
// Challenges expire and are single use.

const enc = new TextEncoder();

function b64url(bytes) {
  let s = '';
  for (const b of bytes) s += String.fromCharCode(b);
  return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

function unb64url(s) {
  const pad = '='.repeat((4 - (s.length % 4)) % 4);
  const bin = atob(s.replace(/-/g, '+').replace(/_/g, '/') + pad);
  return Uint8Array.from(bin, (c) => c.charCodeAt(0));
}

async function hmacKey(secret) {
  return crypto.subtle.importKey('raw', enc.encode(secret), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign', 'verify']);
}

export function leadingZeroBits(bytes) {
  let n = 0;
  for (const b of bytes) {
    if (b === 0) { n += 8; continue; }
    n += Math.clz32(b) - 24;
    break;
  }
  return n;
}

export async function issueChallenge(secret, bits, now = Date.now(), ttlSeconds = 600) {
  const nonce = crypto.getRandomValues(new Uint8Array(16));
  const payload = b64url(enc.encode(JSON.stringify({ n: b64url(nonce), e: Math.floor(now / 1000) + ttlSeconds, b: bits })));
  const sig = new Uint8Array(await crypto.subtle.sign('HMAC', await hmacKey(secret), enc.encode(payload)));
  return { challenge: `${payload}.${b64url(sig)}`, bits, expires: Math.floor(now / 1000) + ttlSeconds };
}

/**
 * Verify a solved challenge.  Returns { ok: true, nonce } or { ok: false, reason }.
 * Single use is enforced by the caller (it must remember `nonce` until it expires).
 */
export async function verifySolution(secret, minBits, challenge, counter, now = Date.now()) {
  if (typeof challenge !== 'string' || typeof counter !== 'string' || !/^[0-9]{1,15}$/.test(counter)) {
    return { ok: false, reason: 'malformed' };
  }
  const dot = challenge.indexOf('.');
  if (dot < 1) return { ok: false, reason: 'malformed' };
  const payload = challenge.slice(0, dot);
  let meta;
  try {
    const sig = unb64url(challenge.slice(dot + 1));
    const good = await crypto.subtle.verify('HMAC', await hmacKey(secret), sig, enc.encode(payload));
    if (!good) return { ok: false, reason: 'bad signature' };
    meta = JSON.parse(new TextDecoder().decode(unb64url(payload)));
  } catch {
    return { ok: false, reason: 'malformed' };
  }
  if (!meta || typeof meta.n !== 'string' || !Number.isInteger(meta.e) || !Number.isInteger(meta.b)) {
    return { ok: false, reason: 'malformed' };
  }
  if (meta.e * 1000 < now) return { ok: false, reason: 'expired' };
  if (meta.b < minBits) return { ok: false, reason: 'weak challenge' };
  const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', enc.encode(`${challenge}:${counter}`)));
  if (leadingZeroBits(digest) < meta.b) return { ok: false, reason: 'insufficient work' };
  return { ok: true, nonce: meta.n, expires: meta.e };
}

/** Reference solver (used by tests; the app has its own implementation). */
export async function solve(challenge, bits) {
  for (let i = 0; ; i++) {
    const d = new Uint8Array(await crypto.subtle.digest('SHA-256', enc.encode(`${challenge}:${i}`)));
    if (leadingZeroBits(d) >= bits) return String(i);
  }
}
