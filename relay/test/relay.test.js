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

// ---- GitHub-backed limits (the relay has no storage of its own) -----------------------------------------------------

import { claimReport } from '../src/limits.js';

const track = (reporter, nonce) => `<!-- glabels-neo-report:v1 -->\n<!-- reporter:${reporter} nonce:${nonce} -->\nbody`;

function githubEnv(extra = {}) {
  return { GITHUB_TOKEN: 't', GITHUB_REPO: 'o/r', LIMITS_BACKEND: 'github', ...extra };
}

function listing(rows) {
  return async () => new Response(JSON.stringify(rows), { status: 200 });
}

test('github limits: a nonce already in a filed report is refused; a new one is accepted', async () => {
  const now = Date.now();
  const rows = [{ created_at: new Date(now - 60_000).toISOString(), body: track('aaaaaaaaaaaa', 'nonce-used-001') }];
  assert.deepEqual(await claimReport(githubEnv(), { nonce: 'nonce-used-001', reporter: 'bbbbbbbbbbbb' }, now, listing(rows)), { ok: false, kind: 'nonce' });
  assert.equal((await claimReport(githubEnv(), { nonce: 'nonce-fresh-002', reporter: 'bbbbbbbbbbbb' }, now, listing(rows))).ok, true);
});

test('github limits: per-reporter hourly cap, other reporters unaffected, old reports ignored', async () => {
  const now = Date.now();
  const mk = (ageMs, reporter, n) => ({ created_at: new Date(now - ageMs).toISOString(), body: track(reporter, `nonce-${n}-0000`) });
  const rows = [mk(60_000, 'aaaaaaaaaaaa', 1), mk(120_000, 'aaaaaaaaaaaa', 2), mk(180_000, 'aaaaaaaaaaaa', 3), mk(2 * 86_400_000, 'cccccccccccc', 4)];
  const env = githubEnv({ LIMIT_PER_HOUR: '3' });
  const limited = await claimReport(env, { nonce: 'nonce-new-0001', reporter: 'aaaaaaaaaaaa' }, now, listing(rows));
  assert.equal(limited.ok, false);
  assert.equal(limited.scope, 'reporter');
  assert.equal((await claimReport(env, { nonce: 'nonce-new-0002', reporter: 'bbbbbbbbbbbb' }, now, listing(rows))).ok, true);
});

test('github limits: the global daily cap is a circuit breaker', async () => {
  const now = Date.now();
  const rows = Array.from({ length: 5 }, (_, i) => ({ created_at: new Date(now - 1000 * (i + 1)).toISOString(), body: track(`${i}`.repeat(12), `nonce-g-${i}-0000`) }));
  const r = await claimReport(githubEnv({ LIMIT_GLOBAL_PER_DAY: '5' }), { nonce: 'nonce-new-0003', reporter: 'dddddddddddd' }, now, listing(rows));
  assert.equal(r.ok, false);
  assert.equal(r.scope, 'global');
});

test('github limits: when GitHub cannot be reached the report is refused (fail closed)', async () => {
  resetMemoryLimits();
  const res = await postReport({}, {}, { createIssue: async () => ({ number: 1, url: 'u' }) }, env({ LIMITS_BACKEND: 'github' }));
  // postReport uses the default fetch, which has no network or token here: it must not file anything.
  assert.ok([503, 502].includes(res.status));
});

test('formatIssue embeds validated tracking markers only', () => {
  const r = validateReport({ schema: 1, description: 'x'.repeat(20) });
  assert.match(formatIssue(r, { reporter: 'abcdef012345', nonce: 'AbC_-123xyz' }).body, /<!-- reporter:abcdef012345 nonce:AbC_-123xyz -->/);
  assert.ok(!formatIssue(r, { reporter: 'x --> <b>', nonce: 'n' }).body.includes('reporter:'));
});

// ---- DigitalOcean Functions adapter ----------------------------------------------------------------------------------

test('DO adapter: raw web invocation round trip (health, challenge, client address, settings)', async () => {
  const { execFileSync } = await import('node:child_process');
  const { fileURLToPath } = await import('node:url');
  execFileSync(process.execPath, [fileURLToPath(new URL('../do/stage.mjs', import.meta.url))]);
  const adapter = await import('../do/packages/relay/report/adapter.mjs');

  assert.equal(adapter.clientIp({ 'x-forwarded-for': '1.2.3.4, 9.9.9.9' }), '9.9.9.9'); // only the edge's entry is trusted
  assert.equal(adapter.clientIp({}), 'unknown');
  assert.deepEqual(adapter.settingsFrom({ ENABLED: 'true', EVIL: 'x', POW_BITS: 18 }, {}), { ENABLED: 'true', POW_BITS: '18' });

  const health = await adapter.main({ __ow_method: 'get', __ow_path: '/v1/health', __ow_headers: {} });
  assert.equal(health.statusCode, 200);
  assert.equal(JSON.parse(health.body).ok, true);

  // without a signing secret configured the relay refuses to hand out challenges
  const notConfigured = await adapter.main({ __ow_method: 'get', __ow_path: '/v1/challenge', __ow_headers: {} });
  assert.equal(notConfigured.statusCode, 503);

  const challenge = await adapter.main({ __ow_method: 'get', __ow_path: '/v1/challenge', __ow_headers: {}, CHALLENGE_SECRET: 's3cret', POW_BITS: '8' });
  assert.equal(challenge.statusCode, 200);
  assert.ok(JSON.parse(challenge.body).challenge);
});

// ---- reconciliation after filing -------------------------------------------------------------------------------------

import { reconcile } from '../src/limits.js';

const row = (number, ageMs, reporter, nonce, labels = ['from-app']) => ({
  number, created_at: new Date(Date.now() - ageMs).toISOString(), body: track(reporter, nonce), labels: labels.map((name) => ({ name })),
});

test('reconcile: an earlier report with the same challenge wins; the later one withdraws', async () => {
  const rows = [row(20, 5000, 'aaaaaaaaaaaa', 'nonce-dup-0001'), row(21, 4000, 'aaaaaaaaaaaa', 'nonce-dup-0001')];
  const f = listing(rows);
  assert.equal((await reconcile(githubEnv(), { number: 20, nonce: 'nonce-dup-0001', reporter: 'aaaaaaaaaaaa' }, Date.now(), f)).ok, true);
  const later = await reconcile(githubEnv(), { number: 21, nonce: 'nonce-dup-0001', reporter: 'aaaaaaaaaaaa' }, Date.now(), f);
  assert.equal(later.ok, false);
  assert.match(later.why, /already submitted/);
});

test('reconcile: a burst beyond the per-reporter hourly limit is cut off after the first N; rejected ones do not count', async () => {
  const rows = [1, 2, 3, 4, 5].map((n) => row(30 + n, 5000 - n * 100, 'bbbbbbbbbbbb', `nonce-b-${n}-00000`));
  const env = githubEnv({ LIMIT_PER_HOUR: '3' });
  const f = listing(rows);
  const verdicts = await Promise.all(rows.map((r, i) => reconcile(env, { number: r.number, nonce: `nonce-b-${i + 1}-00000`, reporter: 'bbbbbbbbbbbb' }, Date.now(), f)));
  assert.deepEqual(verdicts.map((v) => v.ok), [true, true, true, false, false]);

  const withRejected = [...rows.slice(0, 3), row(40, 1000, 'bbbbbbbbbbbb', 'nonce-b-9-00000', ['from-app', 'relay-rejected'])];
  assert.equal((await claimReport(env, { nonce: 'nonce-new-1234', reporter: 'cccccccccccc' }, Date.now(), listing(withRejected))).ok, true);
});

test('handler withdraws a report that reconciliation rejects (and answers 429)', async () => {
  resetMemoryLimits();
  let closed;
  const res = await postReport({}, {}, {
    claimReport: async () => ({ ok: true }),
    createIssue: async () => ({ number: 77, url: 'u' }),
    reconcile: async () => ({ ok: false, why: 'duplicate' }),
    close: async (_e, n, why) => { closed = { n, why }; },
  }, env({ LIMITS_BACKEND: 'github', RECONCILE_DELAY_MS: '0', GITHUB_TOKEN: 't' }));
  assert.equal(res.status, 429);
  assert.deepEqual(closed, { n: 77, why: 'duplicate' });
});
