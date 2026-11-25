import assert from 'node:assert/strict';
import { test } from 'node:test';

import { DEFAULT_WAIT_MS, LimitError, MAX_WAIT_MS, MIN_WAIT_MS, asLimitError, isLimitMessage, limitUntil } from '../limits.mjs';

const NOW = Date.parse('2026-11-25T12:00:00Z');

test('what providers say when they are at a limit is recognised', () => {
  for (const m of [
    'Claude AI usage limit reached|1764072000', '5-hour limit reached - resets 3pm', 'OpenRouter returned 429', 'HTTP 529 overloaded_error',
    'Too Many Requests', 'rate_limit_exceeded', 'You exceeded your current quota', 'Weekly limit reached', 'insufficient credits', 'Resource exhausted',
    'claude reported an error: Credit balance is too low',
  ]) assert.ok(isLimitMessage(m), m);
});

test('ordinary failures are not limits', () => {
  for (const m of [
    'the model gave no structured answer', 'opencode timed out', 'model deepseek/x is more expensive than the ceiling (prompt 0.00000015/token > 0.00000012)',
    'invalid verdict: risk', 'claude exited 1 without JSON output', 'ENOENT: no such file', 'OpenRouter returned 500', '',
  ]) assert.ok(!isLimitMessage(m), m);
});

test('the wait is read from the message and always kept between 10 minutes and 6 hours', () => {
  assert.equal(limitUntil('try again in 20 minutes', NOW), NOW + 20 * 60_000);
  assert.equal(limitUntil('retry in 2 hours', NOW), NOW + 2 * 3600_000);
  assert.equal(limitUntil('resets 2026-11-25T14:30:00Z', NOW), Date.parse('2026-11-25T14:30:00Z'));
  assert.equal(limitUntil('rate limit', NOW), NOW + DEFAULT_WAIT_MS);
  assert.equal(limitUntil('retry in 5 seconds', NOW), NOW + MIN_WAIT_MS); // never hammer a limit
  assert.equal(limitUntil('retry in 90 hours', NOW), NOW + MAX_WAIT_MS); // never park work forever
  assert.equal(limitUntil('limit|9999999999', NOW), NOW + MAX_WAIT_MS);
  assert.equal(limitUntil('limit|1000000000', NOW), NOW + MIN_WAIT_MS); // a time in the past
  assert.equal(limitUntil(undefined, NOW), NOW + DEFAULT_WAIT_MS);
  const epoch = Math.floor((NOW + 3 * 3600_000) / 1000);
  assert.equal(limitUntil(`usage limit reached|${epoch}`, NOW), epoch * 1000);
});

test('asLimitError wraps limits only and keeps LimitErrors as they are', () => {
  const e = asLimitError(new Error('HTTP 429 too many requests'), NOW);
  assert.ok(e instanceof LimitError);
  assert.equal(e.until, NOW + DEFAULT_WAIT_MS);
  assert.equal(asLimitError(new Error('boom')), null);
  assert.equal(asLimitError('boom'), null);
  const same = new LimitError('x', 1);
  assert.equal(asLimitError(same), same);
  assert.ok(asLimitError('plain string: usage limit reached') instanceof LimitError);
});

test('runModel: every member at a limit is a LimitError (park the work); anything else stays a failure', async () => {
  const { runModel } = await import('../lib.mjs');
  const env = { ...process.env };
  process.env.CLAUDE_CODE_OAUTH_TOKEN = 'test-token-not-real';
  process.env.MODEL_POOL = 'claude-haiku'; // a single member: no network price check
  delete process.env.OPENROUTER_API_KEY;
  const quiet = () => {};
  try {
    await assert.rejects(
      runModel({ stage: 'triage', prompt: 'x', log: quiet, runMember: async () => { throw new Error('Claude AI usage limit reached|' + Math.floor((Date.now() + 3 * 3600_000) / 1000)); } }),
      (e) => e instanceof LimitError && e.until > Date.now() + 2.9 * 3600_000 && /claude-haiku/.test(e.message),
    );
    await assert.rejects(
      runModel({ stage: 'triage', prompt: 'x', log: quiet, runMember: async () => { throw new Error('the model gave no structured answer'); } }),
      (e) => !(e instanceof LimitError) && /no structured answer/.test(e.message),
    );
    const ok = await runModel({ stage: 'triage', prompt: 'x', log: quiet, runMember: async () => ({ text: 'fine', structured: null, cost: 0 }) });
    assert.equal(ok.used, 'claude-haiku');
  } finally {
    for (const k of ['CLAUDE_CODE_OAUTH_TOKEN', 'MODEL_POOL', 'OPENROUTER_API_KEY']) {
      if (env[k] === undefined) delete process.env[k]; else process.env[k] = env[k];
    }
  }
});
