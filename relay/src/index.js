// gLabels Neo report relay.
//
//   GET  /v1/challenge   -> { challenge, bits }            (proof-of-work challenge)
//   POST /v1/reports     -> { ok, number, url }            (file a report as a GitHub issue)
//   GET  /v1/health      -> { ok, enabled }
//
// The app never holds a GitHub credential.  This service does, scoped to creating issues in one
// repository, and it only ever creates issues with the fixed labels below.

import { createIssue } from './github.js';
import { claimReport, sha256Hex } from './limits.js';
import { issueChallenge, verifySolution } from './pow.js';
import { LIMITS, ValidationError, formatIssue, validateReport } from './sanitize.js';

const LABELS = ['bug', 'from-app', 'needs-triage'];

function json(status, body, extra = {}) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store', ...extra },
  });
}

export async function handle(request, env, ctx, deps = {}) {
  const url = new URL(request.url);
  const create = deps.createIssue ?? createIssue;
  const now = deps.now ?? Date.now();

  if (url.pathname === '/v1/health') return json(200, { ok: true, enabled: env.ENABLED !== 'false' });

  if (env.ENABLED === 'false') return json(503, { ok: false, error: 'reporting is paused' });
  if (!env.CHALLENGE_SECRET) return json(503, { ok: false, error: 'relay is not configured' });
  const bits = Number(env.POW_BITS) > 0 ? Number(env.POW_BITS) : 18;

  if (url.pathname === '/v1/challenge' && request.method === 'GET') {
    return json(200, await issueChallenge(env.CHALLENGE_SECRET, bits, now));
  }

  if (url.pathname === '/v1/reports' && request.method === 'POST') {
    const ua = request.headers.get('user-agent') ?? '';
    if (!ua.startsWith('glabels-neo/')) return json(403, { ok: false, error: 'unsupported client' });
    if (!(request.headers.get('content-type') ?? '').startsWith('application/json')) {
      return json(415, { ok: false, error: 'expected application/json' });
    }
    const declared = Number(request.headers.get('content-length') ?? 0);
    if (declared > LIMITS.bodyBytes) return json(413, { ok: false, error: 'report too large' });
    const text = await request.text();
    if (text.length > LIMITS.bodyBytes) return json(413, { ok: false, error: 'report too large' });

    let raw;
    try {
      raw = JSON.parse(text);
    } catch {
      return json(400, { ok: false, error: 'invalid JSON' });
    }

    // 1. proof of work (cheap to verify, so do it before anything that costs)
    const pow = await verifySolution(env.CHALLENGE_SECRET, bits, raw?.pow?.challenge, raw?.pow?.counter, now);
    if (!pow.ok) return json(400, { ok: false, error: `proof of work rejected: ${pow.reason}` });

    // 2. content
    let report;
    try {
      report = validateReport(raw);
    } catch (e) {
      if (e instanceof ValidationError) return json(400, { ok: false, error: e.message });
      throw e;
    }

    // 3. single use + rate limits
    // (cf-connecting-ip is set by Cloudflare itself; behind any other proxy it must be the trusted client address.)
    const ip = request.headers.get('cf-connecting-ip') ?? 'unknown';
    const limit = await claimReport(
      env,
      { nonce: pow.nonce, nonceExpires: pow.expires * 1000, reporter: (await sha256Hex(`${env.CHALLENGE_SECRET}|${ip}`)).slice(0, 24) },
      now,
    );
    if (!limit.ok && limit.kind === 'nonce') return json(400, { ok: false, error: 'challenge already used' });
    if (!limit.ok) {
      return json(
        429,
        { ok: false, error: 'too many reports, please try again later', scope: limit.scope },
        { 'retry-after': String(limit.retryAfter) },
      );
    }

    // 4. file it
    try {
      const issue = await create(env, formatIssue(report), LABELS);
      return json(201, { ok: true, number: issue.number, url: issue.url });
    } catch (e) {
      console.error('issue creation failed:', e instanceof Error ? e.message : e);
      return json(502, { ok: false, error: 'could not reach GitHub' });
    }
  }

  return json(404, { ok: false, error: 'not found' });
}

export { Limiter } from './limiter.js';

export default {
  fetch(request, env, ctx) {
    return handle(request, env, ctx).catch((e) => {
      console.error('unhandled:', e instanceof Error ? e.message : e);
      return json(500, { ok: false, error: 'internal error' });
    });
  },
};
