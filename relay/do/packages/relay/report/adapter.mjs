// Adapts a DigitalOcean Functions "raw web" invocation to the relay's Request/Response handler.

import { webcrypto } from 'node:crypto';

if (!globalThis.crypto) globalThis.crypto = webcrypto;

const { handle } = await import('./src/index.js');

// Settings the relay reads.  On DigitalOcean Functions, `environment:` entries of project.yml arrive as parameters.
const SETTINGS = [
  'ENABLED', 'POW_BITS', 'GITHUB_REPO', 'GITHUB_TOKEN', 'CHALLENGE_SECRET', 'LIMITS_BACKEND',
  'LIMIT_PER_HOUR', 'LIMIT_PER_DAY', 'LIMIT_GLOBAL_PER_DAY', 'RECONCILE_DELAY_MS',
];

export function settingsFrom(args, processEnv = process.env) {
  const env = {};
  for (const k of SETTINGS) {
    const v = args[k] ?? processEnv[k];
    if (v !== undefined && v !== '') env[k] = String(v);
  }
  return env;
}

/** The client address as seen by the platform's edge: the LAST entry of X-Forwarded-For (earlier ones are client-supplied). */
export function clientIp(headers) {
  const xff = String(headers['x-forwarded-for'] ?? '').split(',').map((s) => s.trim()).filter(Boolean);
  return xff.length ? xff[xff.length - 1] : 'unknown';
}

function decodeBody(args) {
  const raw = args.__ow_body;
  if (raw === undefined || raw === null || raw === '') return undefined;
  const text = String(raw);
  try {
    JSON.parse(text);
    return text;
  } catch {
    // raw web actions may base64-encode the body
    const decoded = Buffer.from(text, 'base64').toString('utf8');
    return decoded;
  }
}

export function toRequest(args) {
  const method = String(args.__ow_method ?? 'get').toUpperCase();
  const incoming = args.__ow_headers ?? {};
  const headers = new Headers();
  for (const [k, v] of Object.entries(incoming)) {
    if (k.toLowerCase() === 'x-client-ip') continue; // never trust a client-supplied value
    headers.set(k, String(v));
  }
  headers.set('x-client-ip', clientIp(incoming));
  const path = String(args.__ow_path ?? '/') || '/';
  const query = args.__ow_query ? `?${args.__ow_query}` : '';
  const init = { method, headers };
  if (method !== 'GET' && method !== 'HEAD') init.body = decodeBody(args);
  return new Request(`https://relay.invalid${path.startsWith('/') ? path : `/${path}`}${query}`, init);
}

export async function main(args) {
  try {
    const response = await handle(toRequest(args), settingsFrom(args), {});
    const headers = {};
    response.headers.forEach((v, k) => {
      headers[k] = v;
    });
    return { statusCode: response.status, headers, body: await response.text() };
  } catch (e) {
    console.error('unhandled:', e instanceof Error ? e.message : e);
    return { statusCode: 500, headers: { 'content-type': 'application/json' }, body: '{"ok":false,"error":"internal error"}' };
  }
}
