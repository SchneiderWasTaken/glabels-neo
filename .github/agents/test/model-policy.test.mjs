import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  ALLOWED_CLAUDE, ALLOWED_OPENROUTER, POOL, PRICE_CEILING, STAGES, assertAllowedModel, assertModelRunnable, candidatesFor, checkPricing,
  memberById, resetCatalogue, tokenCostUsd,
} from '../model-policy.mjs';

const GOOD = { id: 'deepseek/deepseek-v4.1-flash-1017', pricing: { prompt: '0.00000003', completion: '0.0000002', request: '0', image: '0', web_search: '0', internal_reasoning: '0' } };
const GLM = { id: 'z-ai/glm-5.3-flash', pricing: { prompt: '0.00000012', completion: '0.00000038' } };
const catalogue = (...entries) => async () => ({ ok: true, status: 200, json: async () => ({ data: entries }) });
const KEYS = { OPENROUTER_API_KEY: 'k1', CLAUDE_CODE_OAUTH_TOKEN: 'k2' };

test('the pool is DeepSeek V4.1 flash 1017, GLM 5.3 flash, and Claude Sonnet, Haiku and Opus - nothing else', () => {
  assert.deepEqual([...ALLOWED_OPENROUTER], ['deepseek/deepseek-v4.1-flash-1017', 'z-ai/glm-5.3-flash']);
  assert.deepEqual([...ALLOWED_CLAUDE], ['sonnet', 'haiku', 'opus']);
  assert.equal(POOL.length, 5);
  // Opus is the most expensive subscription model: only the auto-fix council may use it, as a required reviewer
  for (const stage of ['triage', 'fix', 'council']) assert.ok(!STAGES[stage].includes('claude-opus'), stage);
  assert.deepEqual([...STAGES['council-auto']], ['claude-opus']);
  assert.equal(new Set(POOL.map((m) => m.id)).size, POOL.length);
  for (const ids of Object.values(STAGES)) ids.forEach(memberById); // every stage member exists
  assert.ok(STAGES.fix.includes('claude-sonnet') && !STAGES.fix.includes('claude-haiku'), 'code edits use Sonnet, not Haiku');
});

test('allowed models pass, with or without the openrouter/ prefix', () => {
  assert.deepEqual(assertAllowedModel('openrouter', 'deepseek/deepseek-v4.1-flash-1017'), { provider: 'openrouter', model: 'deepseek/deepseek-v4.1-flash-1017' });
  assert.deepEqual(assertAllowedModel('openrouter', 'openrouter/z-ai/glm-5.3-flash'), { provider: 'openrouter', model: 'z-ai/glm-5.3-flash' });
  assert.deepEqual(assertAllowedModel('claude', 'sonnet'), { provider: 'claude', model: 'sonnet' });
  assert.deepEqual(assertAllowedModel('claude', 'haiku'), { provider: 'claude', model: 'haiku' });
});

test('everything else is refused: other models, aliases, variants, other providers, near misses', () => {
  const refused = [
    ['openrouter', ''], ['openrouter', undefined], ['openrouter', null], ['openrouter', 'sonnet'], ['openrouter', 'anthropic/claude-sonnet-5.5'],
    ['openrouter', 'google/gemini-4-neon'], ['openrouter', 'z-ai/glm-5.3-flashx'], ['openrouter', 'z-ai/glm-5.3-flashx-legal'],
    ['openrouter', '~z-ai/glm-flash-latest'], ['openrouter', '~deepseek/deepseek-flash-latest'], ['openrouter', 'deepseek/deepseek-v4.1-flash'],
    ['openrouter', 'deepseek/deepseek-v4.1-flash-1017:batch'], ['openrouter', 'z-ai/glm-5.3-flash:batch'], ['openrouter', 'x/deepseek/deepseek-v4.1-flash-1017'],
    ['openrouter', 'openrouter/openrouter/z-ai/glm-5.3-flash'], ['openrouter', 'DEEPSEEK/deepseek-v4.1-flash-1017'], ['openrouter', 'opencode/glm-5.3-flash-free'],
    ['claude', 'claude-opus-5-5'], ['claude', 'opus[1m]'], ['claude', 'Opus'], ['claude', 'sonnet[1m]'], ['claude', 'fable'], ['claude', 'deepseek/deepseek-v4.1-flash-1017'],
    ['gemini', 'gemini-4-neon'], ['gemini', 'sonnet'], ['openrouter', 'sonnet '.trim()], ['claude', 'z-ai/glm-5.3-flash'], ['openai', 'gpt'],
  ];
  for (const [provider, model] of refused) assert.throws(() => assertAllowedModel(provider, model), /not permitted/, `${provider}:${model}`);
});

test('candidates: stage pools, secrets and rotation', () => {
  assert.deepEqual(candidatesFor('triage', { env: KEYS, seed: 0 }).map((m) => m.id), ['deepseek', 'glm', 'claude-haiku']);
  assert.deepEqual(candidatesFor('triage', { env: KEYS, seed: 1 }).map((m) => m.id), ['glm', 'claude-haiku', 'deepseek']);
  assert.deepEqual(candidatesFor('triage', { env: KEYS, seed: 2 }).map((m) => m.id), ['claude-haiku', 'deepseek', 'glm']);
  assert.deepEqual(candidatesFor('triage', { env: KEYS, seed: 3 }).map((m) => m.id), ['deepseek', 'glm', 'claude-haiku']);
  assert.deepEqual(candidatesFor('fix', { env: KEYS, seed: 2 }).map((m) => m.id), ['claude-sonnet', 'deepseek', 'glm']);
  assert.deepEqual(candidatesFor('triage', { env: KEYS, seed: -1 }).map((m) => m.id), ['claude-haiku', 'deepseek', 'glm']);
  assert.deepEqual(candidatesFor('triage', { env: KEYS, seed: 'x' }).map((m) => m.id), ['deepseek', 'glm', 'claude-haiku']);
  // work spreads evenly: over many items every member starts about the same number of times
  const starts = {};
  for (let n = 1; n <= 300; n++) starts[candidatesFor('fix', { env: KEYS, seed: n })[0].id] = (starts[candidatesFor('fix', { env: KEYS, seed: n })[0].id] ?? 0) + 1;
  assert.deepEqual(Object.values(starts), [100, 100, 100]);
  // a member without its secret is skipped
  assert.deepEqual(candidatesFor('triage', { env: { OPENROUTER_API_KEY: 'k' }, seed: 2 }).map((m) => m.id), ['deepseek', 'glm']);
  assert.deepEqual(candidatesFor('triage', { env: { CLAUDE_CODE_OAUTH_TOKEN: 'k' } }).map((m) => m.id), ['claude-haiku']);
  assert.throws(() => candidatesFor('triage', { env: {} }), /no model is available/);
  assert.throws(() => candidatesFor('nope', { env: KEYS }), /unknown stage/);
});

test('MODEL_POOL can only narrow the pool, never widen it', () => {
  assert.deepEqual(candidatesFor('triage', { env: { ...KEYS, MODEL_POOL: 'glm,claude-haiku' } }).map((m) => m.id), ['glm', 'claude-haiku']);
  // ids a stage does not allow are ignored: Haiku never edits code
  assert.deepEqual(candidatesFor('fix', { env: { ...KEYS, MODEL_POOL: 'claude-haiku,glm' } }).map((m) => m.id), ['glm']);
  assert.throws(() => candidatesFor('fix', { env: { ...KEYS, MODEL_POOL: 'gemini-neon' } }), /unknown model/);
  assert.throws(() => candidatesFor('fix', { env: { ...KEYS, MODEL_POOL: 'anthropic/claude-opus-5' } }), /unknown model/);
  assert.throws(() => candidatesFor('fix', { env: { ...KEYS, MODEL_POOL: 'claude-haiku' } }), /no model is available/);
});

test('price ceiling: GLM 5.3 flash is the most expensive OpenRouter model allowed', () => {
  checkPricing(GOOD);
  checkPricing(GLM);
  assert.equal(PRICE_CEILING.prompt, 0.00000012);
  assert.equal(PRICE_CEILING.completion, 0.00000038);
  const dearer = (over) => ({ id: 'x', pricing: { ...GLM.pricing, ...over } });
  assert.throws(() => checkPricing(dearer({ prompt: '0.00000013' })), /more expensive/);
  assert.throws(() => checkPricing(dearer({ completion: '0.0000005' })), /more expensive/);
  assert.throws(() => checkPricing(dearer({ internal_reasoning: '0.000001' })), /more expensive/);
  assert.throws(() => checkPricing(dearer({ request: '0.01' })), /more expensive/);
  assert.throws(() => checkPricing(dearer({ web_search: '0.01' })), /more expensive/);
  assert.throws(() => checkPricing(dearer({ prompt: 'abc' })), /more expensive/);
  assert.throws(() => checkPricing({ id: 'x' }), /no pricing/);
  assert.throws(() => checkPricing(undefined), /no pricing/);
});

test('assertModelRunnable: allowlist first, then the live price for OpenRouter, and fail closed', async () => {
  resetCatalogue();
  await assert.rejects(assertModelRunnable('openrouter', 'sonnet', { fetchImpl: catalogue(GOOD) }), /not permitted/);
  await assert.rejects(assertModelRunnable('claude', 'fable'), /not permitted/);
  assert.deepEqual(await assertModelRunnable('claude', 'opus'), { provider: 'claude', model: 'opus' });
  assert.deepEqual(await assertModelRunnable('claude', 'sonnet'), { provider: 'claude', model: 'sonnet' }); // no price lookup for Claude
  assert.deepEqual(await assertModelRunnable('openrouter', 'openrouter/deepseek/deepseek-v4.1-flash-1017', { fetchImpl: catalogue(GOOD, GLM) }), { provider: 'openrouter', model: 'deepseek/deepseek-v4.1-flash-1017' });

  // the price went up since the ceiling was set
  resetCatalogue();
  const pricey = { id: 'z-ai/glm-5.3-flash', pricing: { prompt: '0.0000005', completion: '0.000002' } };
  await assert.rejects(assertModelRunnable('openrouter', 'z-ai/glm-5.3-flash', { fetchImpl: catalogue(pricey) }), /more expensive/);

  // not in the list, listing unreachable, or nonsense answers: refused
  resetCatalogue();
  await assert.rejects(assertModelRunnable('openrouter', 'z-ai/glm-5.3-flash', { fetchImpl: catalogue(GOOD) }), /not in OpenRouter/);
  resetCatalogue();
  await assert.rejects(assertModelRunnable('openrouter', 'z-ai/glm-5.3-flash', { fetchImpl: async () => ({ ok: false, status: 503 }) }), /refusing/);
  resetCatalogue();
  await assert.rejects(assertModelRunnable('openrouter', 'z-ai/glm-5.3-flash', { fetchImpl: async () => { throw new Error('offline'); } }), /refusing/);
  resetCatalogue();
  await assert.rejects(assertModelRunnable('openrouter', 'z-ai/glm-5.3-flash', { fetchImpl: async () => ({ ok: true, status: 200, json: async () => ({ nope: 1 }) }) }), /refusing/);
  resetCatalogue();
});

test('token cost is priced at the ceiling, cache and reasoning included', () => {
  assert.equal(tokenCostUsd({}), 0);
  assert.ok(Math.abs(tokenCostUsd({ input: 1_000_000 }) - 0.12) < 1e-9);
  assert.ok(Math.abs(tokenCostUsd({ output: 1_000_000 }) - 0.38) < 1e-9);
  assert.ok(Math.abs(tokenCostUsd({ reasoning: 1_000_000 }) - 0.38) < 1e-9);
  assert.ok(Math.abs(tokenCostUsd({ cacheRead: 1_000_000, cacheWrite: 1_000_000 }) - 0.24) < 1e-9);
});
