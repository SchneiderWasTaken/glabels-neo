import assert from 'node:assert/strict';
import { generateKeyPairSync, createVerify } from 'node:crypto';
import { test } from 'node:test';

import { appJwt, getToken, resetTokenCache } from '../src/github.js';
import { handle } from '../src/index.js';
import { resetMemoryLimits } from '../src/limits.js';
import { issueChallenge, solve, verifySolution } from '../src/pow.js';
import { MARKER, ValidationError, fenceFor, formatIssue, neutralizeMarkdown, validateReport } from '../src/sanitize.js';

const SECRET = 'test-secret';
const BITS = 8; // keep tests fast

function env(extra = {}) {
  return { CHALLENGE_SECRET: SECRET, POW_BITS: String(BITS), GITHUB_REPO: 'o/r', GITHUB_TOKEN: 't', ...extra };
}

async function postReport(overrides = {}, headers = {}, deps = {}, e = env()) {
  const { challenge } = await issueChallenge(SECRET, BITS);
  const counter = await solve(challenge, BITS);
  const body = {
    schema: 1,
    pow: { challenge, counter },
    summary: 'Crash when printing',
    description: 'It crashes when I press print on a two column sheet.',
    diagnostics: '> GLABELS NEO\n>     Version: 1.2.3',
    version: '1.2.3',
    attachments: [],
    ...overrides,
  };
  const req = new Request('https://relay.example/v1/reports', {
    method: 'POST',
    headers: { 'content-type': 'application/json', 'user-agent': 'glabels-neo/1.2.3', 'cf-connecting-ip': '203.0.113.9', ...headers },
    body: JSON.stringify(body),
  });
  return handle(req, e, {}, deps);
}

test('neutralizeMarkdown breaks mentions and forged markers', () => {
  const out = neutralizeMarkdown('hi @octocat <!-- glabels-neo-report:v1 --> <script>');
  assert.ok(!out.includes('@octocat'));
  assert.ok(!out.includes('<!--'));
  assert.ok(!out.includes('<script'));
});

test('fenceFor outgrows backtick runs in the content', () => {
  assert.equal(fenceFor('plain'), '```');
  assert.equal(fenceFor('a ```` b'), '`````');
});

test('formatIssue: marker once, content cannot close the fence, attachments become comments', () => {
  const r = validateReport({
    schema: 1,
    description: 'x'.repeat(20),
    diagnostics: 'ok\n```\n## injected heading\n```',
    attachments: [{ kind: 'label', name: 'a b.glabels', text: '<x>```</x>' }],
  });
  const { title, body, comments } = formatIssue(r);
  assert.match(title, /^\[Bug\]: /);
  assert.equal(body.split(MARKER).length - 1, 1);
  assert.equal(comments.length, 1);
  assert.match(comments[0], /<details>/);
  assert.match(comments[0], /````xml/);
});

test('validateReport rejects bad input', () => {
  assert.throws(() => validateReport(null), ValidationError);
  assert.throws(() => validateReport({ schema: 2, description: 'long enough text' }), ValidationError);
  assert.throws(() => validateReport({ schema: 1, description: 'short' }), ValidationError);
  assert.throws(
    () => validateReport({ schema: 1, description: 'long enough text', attachments: [{ kind: 'exe', text: 'x' }] }),
    ValidationError,
  );
  assert.throws(
    () =>
      validateReport({
        schema: 1,
        description: 'long enough text',
        attachments: [1, 2, 3].map(() => ({ kind: 'log', text: 'x' })),
      }),
    ValidationError,
  );
});

test('validateReport strips control characters and caps length', () => {
  const r = validateReport({ schema: 1, description: 'a' + String.fromCharCode(0) + 'b' + String.fromCharCode(0x202e) + 'c' + 'z'.repeat(9000), summary: ['line1', 'line2'].join(String.fromCharCode(10)) });
  assert.ok(!/[\u0000\u202e]/.test(r.description));
  assert.ok(r.description.length <= 8000);
  assert.equal(r.summary, 'line1 line2');
});

test('proof of work: valid, tampered, expired, weak, replayed counter', async () => {
  const { challenge } = await issueChallenge(SECRET, BITS, Date.now());
  const counter = await solve(challenge, BITS);
  assert.equal((await verifySolution(SECRET, BITS, challenge, counter)).ok, true);
  assert.equal((await verifySolution(SECRET, BITS, challenge + 'x', counter)).ok, false);
  assert.equal((await verifySolution('other', BITS, challenge, counter)).ok, false);
  assert.equal((await verifySolution(SECRET, BITS, challenge, 'abc')).ok, false);
  assert.equal((await verifySolution(SECRET, BITS, challenge, counter, Date.now() + 3600_000)).reason, 'expired');
  const weak = await issueChallenge(SECRET, 2);
  assert.equal((await verifySolution(SECRET, BITS, weak.challenge, await solve(weak.challenge, 2))).reason, 'weak challenge');
});

test('GET /v1/challenge then POST /v1/reports files an issue', async () => {
  resetMemoryLimits();
  let seen;
  const res = await postReport({}, {}, {
    createIssue: async (_e, issue, labels) => {
      seen = { issue, labels };
      return { number: 42, url: 'https://github.com/o/r/issues/42' };
    },
  });
  assert.equal(res.status, 201);
  assert.deepEqual(await res.json(), { ok: true, number: 42, url: 'https://github.com/o/r/issues/42' });
  assert.deepEqual(seen.labels, ['bug', 'from-app', 'needs-triage']);
  assert.match(seen.issue.body, new RegExp(MARKER));
});

test('client cannot choose labels or title prefix', async () => {
  resetMemoryLimits();
  let seen;
  await postReport({ labels: ['agent-fix'], summary: 'x' }, {}, {
    createIssue: async (_e, issue, labels) => {
      seen = { issue, labels };
      return { number: 1, url: 'u' };
    },
  });
  assert.ok(!seen.labels.includes('agent-fix'));
});

test('rejects unknown clients, bad content type, bad JSON, oversize', async () => {
  resetMemoryLimits();
  assert.equal((await postReport({}, { 'user-agent': 'curl/8' })).status, 403);
  assert.equal((await postReport({}, { 'content-type': 'text/plain' })).status, 415);
  const bad = await handle(
    new Request('https://relay.example/v1/reports', {
      method: 'POST',
      headers: { 'content-type': 'application/json', 'user-agent': 'glabels-neo/1' },
      body: '{nope',
    }),
    env(),
    {},
  );
  assert.equal(bad.status, 400);
  const big = await handle(
    new Request('https://relay.example/v1/reports', {
      method: 'POST',
      headers: { 'content-type': 'application/json', 'user-agent': 'glabels-neo/1', 'content-length': '999999' },
      body: '{}',
    }),
    env(),
    {},
  );
  assert.equal(big.status, 413);
});

test('a challenge can be used once', async () => {
  resetMemoryLimits();
  const { challenge } = await issueChallenge(SECRET, BITS);
  const counter = await solve(challenge, BITS);
  const make = () =>
    new Request('https://relay.example/v1/reports', {
      method: 'POST',
      headers: { 'content-type': 'application/json', 'user-agent': 'glabels-neo/1', 'cf-connecting-ip': '198.51.100.1' },
      body: JSON.stringify({ schema: 1, pow: { challenge, counter }, description: 'long enough description' }),
    });
  const deps = { createIssue: async () => ({ number: 1, url: 'u' }) };
  assert.equal((await handle(make(), env(), {}, deps)).status, 201);
  const second = await handle(make(), env(), {}, deps);
  assert.equal(second.status, 400);
  assert.match((await second.json()).error, /already used/);
});

test('per-reporter limit returns 429 with Retry-After; other reporters are unaffected', async () => {
  resetMemoryLimits();
  const deps = { createIssue: async () => ({ number: 1, url: 'u' }) };
  const e = env({ LIMIT_PER_HOUR: '2' });
  assert.equal((await postReport({}, {}, deps, e)).status, 201);
  assert.equal((await postReport({}, {}, deps, e)).status, 201);
  const limited = await postReport({}, {}, deps, e);
  assert.equal(limited.status, 429);
  assert.ok(Number(limited.headers.get('retry-after')) > 0);
  assert.equal((await postReport({}, { 'cf-connecting-ip': '203.0.113.77' }, deps, e)).status, 201);
});

test('global limit is a circuit breaker', async () => {
  resetMemoryLimits();
  const deps = { createIssue: async () => ({ number: 1, url: 'u' }) };
  const e = env({ LIMIT_GLOBAL_PER_DAY: '1' });
  assert.equal((await postReport({}, { 'cf-connecting-ip': '192.0.2.1' }, deps, e)).status, 201);
  const res = await postReport({}, { 'cf-connecting-ip': '192.0.2.2' }, deps, e);
  assert.equal(res.status, 429);
  assert.equal((await res.json()).scope, 'global');
});

test('ENABLED=false pauses reporting but health still answers', async () => {
  const e = env({ ENABLED: 'false' });
  assert.equal((await handle(new Request('https://relay.example/v1/challenge'), e, {})).status, 503);
  const h = await handle(new Request('https://relay.example/v1/health'), e, {});
  assert.equal(h.status, 200);
  assert.equal((await h.json()).enabled, false);
});

test('GitHub failure maps to 502 without leaking details', async () => {
  resetMemoryLimits();
  const res = await postReport({}, {}, {
    createIssue: async () => {
      throw new Error('secret detail');
    },
  });
  assert.equal(res.status, 502);
  assert.ok(!JSON.stringify(await res.json()).includes('secret detail'));
});

test('GitHub App auth: PKCS#1 key is accepted and the JWT verifies', async () => {
  const { privateKey, publicKey } = generateKeyPairSync('rsa', { modulusLength: 2048 });
  const pkcs1 = privateKey.export({ type: 'pkcs1', format: 'pem' });
  const pkcs8 = privateKey.export({ type: 'pkcs8', format: 'pem' });
  for (const pem of [pkcs1, pkcs8]) {
    const jwt = await appJwt(1234, pem);
    const [h, p, s] = jwt.split('.');
    const v = createVerify('RSA-SHA256');
    v.update(`${h}.${p}`);
    assert.equal(v.verify(publicKey, Buffer.from(s, 'base64url')), true);
    assert.equal(JSON.parse(Buffer.from(p, 'base64url')).iss, '1234');
  }
});

test('installation token is requested with minimal permissions and cached', async () => {
  resetTokenCache();
  const { privateKey } = generateKeyPairSync('rsa', { modulusLength: 2048 });
  const calls = [];
  const fetchFn = async (url, init) => {
    calls.push({ url, body: JSON.parse(init.body) });
    return new Response(JSON.stringify({ token: 'ghs_x', expires_at: new Date(Date.now() + 3600_000).toISOString() }), {
      status: 201,
    });
  };
  const e = {
    GITHUB_REPO: 'owner/repo',
    GITHUB_APP_ID: '1',
    GITHUB_APP_INSTALLATION_ID: '99',
    GITHUB_APP_PRIVATE_KEY: privateKey.export({ type: 'pkcs1', format: 'pem' }),
  };
  assert.equal(await getToken(e, fetchFn), 'ghs_x');
  assert.equal(await getToken(e, fetchFn), 'ghs_x');
  assert.equal(calls.length, 1);
  assert.match(calls[0].url, /installations\/99\/access_tokens$/);
  assert.deepEqual(calls[0].body, { repositories: ['repo'], permissions: { issues: 'write' } });
});

test('neutralizeMarkdown defuses images, links and cross-repo references', () => {
  const out = neutralizeMarkdown('![x](http://t.example/p.png) [click](http://e.example) see other/repo#12');
  assert.ok(!out.includes('!['));
  assert.ok(!out.includes(']('));
  assert.ok(!/other\/repo#12/.test(out));
});

test('a huge run of backticks cannot blow past the fence or the size limit', () => {
  const { comments } = formatIssue(validateReport({
    schema: 1, description: 'x'.repeat(20), attachments: [{ kind: 'log', name: 'l', text: '`'.repeat(60000) }],
  }));
  assert.ok(comments[0].length < 65000);
});

test('Durable Object limiter: a nonce is claimed once, limits hold under concurrency', async () => {
  const { Limiter } = await import('../src/limiter.js');
  const data = new Map();
  const state = {
    storage: {
      get: async (k) => data.get(k),
      put: async (obj) => { for (const [k, v] of Object.entries(obj)) data.set(k, v); },
      getAlarm: async () => null,
      setAlarm: async () => {},
      deleteAll: async () => data.clear(),
    },
  };
  const limiter = new Limiter(state);
  // A real Durable Object serialises requests; emulate that with a promise chain.
  let chain = Promise.resolve();
  const env = {
    LIMITER: {
      idFromName: () => 'global',
      get: () => ({ fetch: (url, init) => (chain = chain.then(() => limiter.fetch(new Request(url, init)))) }),
    },
    LIMIT_PER_HOUR: '2',
  };
  const { claimReport } = await import('../src/limits.js');
  const now = Date.now();
  const claim = (nonce) => claimReport(env, { nonce, nonceExpires: now + 600_000, reporter: 'r1' }, now);
  const results = await Promise.all([claim('a'), claim('a'), claim('b'), claim('c')]);
  assert.equal(results.filter((r) => r.ok).length, 2); // the duplicate nonce and the third report are refused
  assert.ok(results.some((r) => r.kind === 'nonce'));
  assert.ok(results.some((r) => r.kind === 'limit'));
});
