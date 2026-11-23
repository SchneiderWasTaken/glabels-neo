// Minimal GitHub client: authenticates as a GitHub App (or with a fine-grained token) and files issues.
// Uses only fetch and WebCrypto, so it runs unchanged on Node, DigitalOcean Functions and Cloudflare Workers.

const API = 'https://api.github.com';
const enc = new TextEncoder();

function b64url(bytes) {
  let s = '';
  for (const b of bytes) s += String.fromCharCode(b);
  return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

// ---- DER helpers: GitHub issues PKCS#1 keys; WebCrypto only imports PKCS#8 ----------------------------

function derLength(n) {
  if (n < 0x80) return Uint8Array.of(n);
  const bytes = [];
  for (let v = n; v > 0; v >>= 8) bytes.unshift(v & 0xff);
  return Uint8Array.of(0x80 | bytes.length, ...bytes);
}

function der(tag, ...parts) {
  const len = parts.reduce((s, p) => s + p.length, 0);
  const lenBytes = derLength(len);
  const out = new Uint8Array(1 + lenBytes.length + len);
  out[0] = tag;
  out.set(lenBytes, 1);
  let o = 1 + lenBytes.length;
  for (const p of parts) {
    out.set(p, o);
    o += p.length;
  }
  return out;
}

export function pemToDer(pem) {
  const body = pem.replace(/-----[^-]+-----/g, '').replace(/\s+/g, '');
  return Uint8Array.from(atob(body), (c) => c.charCodeAt(0));
}

/** Wrap a PKCS#1 RSAPrivateKey in a PKCS#8 PrivateKeyInfo. */
export function pkcs1ToPkcs8(pkcs1) {
  const version = Uint8Array.of(0x02, 0x01, 0x00);
  const rsaOid = Uint8Array.of(0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01);
  const algorithm = der(0x30, rsaOid, Uint8Array.of(0x05, 0x00));
  return der(0x30, version, algorithm, der(0x04, pkcs1));
}

export async function importAppKey(pem) {
  let keyDer = pemToDer(pem);
  if (/BEGIN RSA PRIVATE KEY/.test(pem)) keyDer = pkcs1ToPkcs8(keyDer);
  return crypto.subtle.importKey('pkcs8', keyDer, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, false, ['sign']);
}

export async function appJwt(appId, pem, now = Date.now()) {
  const iat = Math.floor(now / 1000) - 60;
  const header = b64url(enc.encode(JSON.stringify({ alg: 'RS256', typ: 'JWT' })));
  const payload = b64url(enc.encode(JSON.stringify({ iat, exp: iat + 600, iss: String(appId) })));
  const signing = `${header}.${payload}`;
  const sig = new Uint8Array(await crypto.subtle.sign('RSASSA-PKCS1-v1_5', await importAppKey(pem), enc.encode(signing)));
  return `${signing}.${b64url(sig)}`;
}

// ---- API ----------------------------------------------------------------------------------------------

function headers(token) {
  return {
    authorization: `Bearer ${token}`,
    accept: 'application/vnd.github+json',
    'x-github-api-version': '2022-11-28',
    'user-agent': 'glabels-neo-report-relay',
    'content-type': 'application/json',
  };
}

let cached = null; // { token, exp }

/** A token that may create issues in env.GITHUB_REPO and nothing else. */
export async function getToken(env, fetchFn = fetch, now = Date.now()) {
  if (env.GITHUB_TOKEN) return env.GITHUB_TOKEN;
  if (!env.GITHUB_APP_ID || !env.GITHUB_APP_PRIVATE_KEY || !env.GITHUB_APP_INSTALLATION_ID) {
    throw new Error('relay is not configured with GitHub credentials');
  }
  if (cached && cached.exp - 120000 > now) return cached.token;
  const jwt = await appJwt(env.GITHUB_APP_ID, env.GITHUB_APP_PRIVATE_KEY, now);
  const repoName = env.GITHUB_REPO.split('/')[1];
  const res = await fetchFn(`${API}/app/installations/${env.GITHUB_APP_INSTALLATION_ID}/access_tokens`, {
    method: 'POST',
    headers: headers(jwt),
    body: JSON.stringify({ repositories: [repoName], permissions: { issues: 'write' } }),
  });
  if (!res.ok) throw new Error(`installation token request failed: ${res.status}`);
  const data = await res.json();
  cached = { token: data.token, exp: Date.parse(data.expires_at) };
  return cached.token;
}

export function resetTokenCache() {
  cached = null;
}

/** Create the issue and its attachment comments.  Returns { number, url }. */
export async function createIssue(env, { title, body, comments }, labels, fetchFn = fetch) {
  const token = await getToken(env, fetchFn);
  const res = await fetchFn(`${API}/repos/${env.GITHUB_REPO}/issues`, {
    method: 'POST',
    headers: headers(token),
    body: JSON.stringify({ title, body, labels }),
  });
  if (!res.ok) throw new Error(`issue creation failed: ${res.status}`);
  const issue = await res.json();
  for (const text of comments ?? []) {
    // A failed attachment comment must not lose the report itself.
    await fetchFn(`${API}/repos/${env.GITHUB_REPO}/issues/${issue.number}/comments`, {
      method: 'POST',
      headers: headers(token),
      body: JSON.stringify({ body: text }),
    }).catch(() => {});
  }
  return { number: issue.number, url: issue.html_url };
}

/**
 * Reports filed by the relay since `sinceMs` (issues labelled "from-app"), newest first: [{ created_at, body }].
 * Used as the relay's own state (see limits.js).  Throws when GitHub cannot be reached.
 */
export async function listRecentReports(env, sinceMs, fetchFn = fetch) {
  const token = await getToken(env, fetchFn);
  const since = new Date(sinceMs).toISOString();
  const url = `${API}/repos/${env.GITHUB_REPO}/issues?labels=from-app&state=all&since=${encodeURIComponent(since)}` +
    '&sort=created&direction=desc&per_page=100';
  const res = await fetchFn(url, { headers: headers(token) });
  if (!res.ok) throw new Error(`listing recent reports failed: ${res.status}`);
  const issues = await res.json();
  return issues
    .filter((i) => !i.pull_request && Date.parse(i.created_at) >= sinceMs)
    .map((i) => ({ number: i.number, created_at: i.created_at, body: i.body ?? '', labels: (i.labels ?? []).map((l) => (typeof l === 'string' ? l : l.name)) }));
}

/** Close an issue the relay has just filed because it turned out to be a duplicate or over the limits. */
export async function closeAsRejected(env, number, why, fetchFn = fetch) {
  const token = await getToken(env, fetchFn);
  const base = `${API}/repos/${env.GITHUB_REPO}/issues/${number}`;
  await fetchFn(`${base}/comments`, { method: 'POST', headers: headers(token), body: JSON.stringify({ body: `Closed automatically by the report relay: ${why}` }) });
  await fetchFn(`${base}/labels`, { method: 'POST', headers: headers(token), body: JSON.stringify({ labels: ['relay-rejected'] }) });
  const res = await fetchFn(base, { method: 'PATCH', headers: headers(token), body: JSON.stringify({ state: 'closed', state_reason: 'not_planned' }) });
  if (!res.ok) throw new Error(`closing the issue failed: ${res.status}`);
}
