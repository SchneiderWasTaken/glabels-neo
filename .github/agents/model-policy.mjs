// Which models the automation may run, and what they may cost.  Nothing in the pipeline (triage, fixes, council) may
// call a model that is not in POOL, whatever a repository variable, an environment variable or a model's own output says.
//
// The pool spreads the work over five models so that no single budget or usage limit carries it all:
//   - two cheap OpenRouter models (DeepSeek V4.1 flash 1017, GLM 5.3 flash), paid per token;
//   - Claude Sonnet and Haiku, on the maintainer's Claude subscription (usage limits, no per-token bill), and Claude Opus,
//     only as the required reviewer of `auto-fix` pull requests.
// Nothing else, in particular no other OpenRouter model: the ceiling for per-token spending is what DeepSeek V4.1 flash 1017 costs.
//
// Gates, all checked before every model call and all fail-closed:
//   1. an exact-match allowlist (no aliases such as "~...-latest", no ":variant" suffixes, no other providers);
//   2. for OpenRouter models, the live per-token prices must not exceed the ceiling (DeepSeek V4.1 flash 1017 today); if the
//      prices cannot be read, the call is refused;
//   3. per run, a budget that stops a run as soon as the tokens it has streamed would cost more than it (opencode-lib.mjs).

/** The pool.  Changing it is a deliberate, reviewed code change. */
export const POOL = Object.freeze([
  Object.freeze({ id: 'deepseek', provider: 'openrouter', model: 'deepseek/deepseek-v4.1-flash-1017' }),
  Object.freeze({ id: 'glm', provider: 'openrouter', model: 'z-ai/glm-5.3-flash' }),
  Object.freeze({ id: 'claude-sonnet', provider: 'claude', model: 'sonnet' }),
  Object.freeze({ id: 'claude-haiku', provider: 'claude', model: 'haiku' }),
  // Opus reviews only `auto-fix` pull requests (STAGES['council-auto']): it is the most expensive subscription model.
  Object.freeze({ id: 'claude-opus', provider: 'claude', model: 'opus' }),
]);

/** Which pool members each stage may use.  Haiku is for reading tasks; code edits use Sonnet. */
export const STAGES = Object.freeze({
  triage: ['deepseek', 'glm', 'claude-haiku'],
  fix: ['deepseek', 'glm', 'claude-sonnet'],
  council: ['deepseek', 'glm', 'claude-haiku'],
  // Reviewers added to the council for an `auto-fix` pull request, all REQUIRED to take part and approve (council-lib.mjs).
  'council-auto': ['claude-opus'],
});

/** The secret each provider needs; a member without it is skipped. */
export const NEEDS = Object.freeze({ openrouter: 'OPENROUTER_API_KEY', claude: 'CLAUDE_CODE_OAUTH_TOKEN' });

/** OpenRouter slugs that may run (derived from the pool, so there is one list). */
export const ALLOWED_OPENROUTER = Object.freeze(POOL.filter((m) => m.provider === 'openrouter').map((m) => m.model));
export const ALLOWED_CLAUDE = Object.freeze(POOL.filter((m) => m.provider === 'claude').map((m) => m.model));

/** USD per token: DeepSeek V4.1 flash 1017 on OpenRouter ($0.15 / $0.60 per million tokens), the dearest model in the pool.  No OpenRouter model may cost more. */
export const PRICE_CEILING = Object.freeze({ prompt: 0.00000015, completion: 0.0000006 });

const MODELS_URL = 'https://openrouter.ai/api/v1/models';

/** The pool member for an id.  Throws on an unknown id. */
export function memberById(id) {
  const m = POOL.find((x) => x.id === id);
  if (!m) throw new Error(`unknown model "${id}"; the pool is ${POOL.map((x) => x.id).join(', ')}`);
  return m;
}

/**
 * Exact-match check of a provider and model.  Accepts the OpenRouter slug with or without an "openrouter/" prefix.
 * Returns the canonical { provider, model }.  Throws if it is not in the pool.
 */
export function assertAllowedModel(provider, model) {
  const s = String(model ?? '').trim();
  const bare = provider === 'openrouter' && s.startsWith('openrouter/') ? s.slice('openrouter/'.length) : s;
  const found = POOL.find((m) => m.provider === provider && m.model === bare);
  if (!found) throw new Error(`model "${provider}:${s}" is not permitted; the automation may only run ${POOL.map((m) => m.provider + ':' + m.model).join(', ')}`);
  return { provider: found.provider, model: found.model };
}

/**
 * The members a stage may use right now, in the order to try them.
 *   env.MODEL_POOL: optional comma-separated member ids to restrict the stage to (unknown ids are an error; ids a stage
 *                   does not allow are ignored).  It can only narrow the pool, never widen it.
 *   env secrets:    a member whose provider's secret is missing is skipped.
 *   seed:           rotates the starting point (issue number, PR number...), so work spreads evenly and a retry of the
 *                   same item starts on the same member.
 */
export function candidatesFor(stage, { env = process.env, seed = 0 } = {}) {
  if (!STAGES[stage]) throw new Error(`unknown stage "${stage}"`);
  let ids = STAGES[stage];
  const wanted = String(env.MODEL_POOL ?? '').split(',').map((s) => s.trim()).filter(Boolean);
  if (wanted.length) {
    wanted.forEach(memberById); // unknown id: error
    ids = ids.filter((id) => wanted.includes(id));
  }
  const usable = ids.map(memberById).filter((m) => env[NEEDS[m.provider]]);
  if (!usable.length) throw new Error(`no model is available for ${stage}: needs ${[...new Set(ids.map((id) => NEEDS[memberById(id).provider]))].join(' or ')}`);
  const start = ((Number(seed) || 0) % usable.length + usable.length) % usable.length;
  return [...usable.slice(start), ...usable.slice(0, start)];
}

const priceOf = (v) => (v === undefined || v === null || v === '' ? 0 : Number(v));

/**
 * Check one OpenRouter model's prices against the ceiling.  `entry` is OpenRouter's model record.  Anything unreadable
 * fails.  Also refuses per-request, per-search or per-image charges, which the token ceiling would not catch.
 */
export function checkPricing(entry) {
  const p = entry?.pricing;
  if (!p) throw new Error('no pricing information for the model; refusing to run it');
  const prompt = priceOf(p.prompt);
  const completion = priceOf(p.completion);
  const reasoning = priceOf(p.internal_reasoning);
  const bad = [];
  if (!Number.isFinite(prompt) || prompt > PRICE_CEILING.prompt) bad.push(`prompt ${p.prompt}/token > ${PRICE_CEILING.prompt}`);
  if (!Number.isFinite(completion) || completion > PRICE_CEILING.completion) bad.push(`completion ${p.completion}/token > ${PRICE_CEILING.completion}`);
  if (!Number.isFinite(reasoning) || reasoning > PRICE_CEILING.completion) bad.push(`reasoning ${p.internal_reasoning}/token > ${PRICE_CEILING.completion}`);
  for (const k of ['request', 'image', 'web_search']) {
    const v = priceOf(p[k]);
    if (!Number.isFinite(v) || v > 0) bad.push(`${k} charge ${p[k]}`);
  }
  if (bad.length) throw new Error(`model ${entry.id} is more expensive than the ceiling (${bad.join('; ')}); refusing to run it`);
}

let catalogue; // fetched once per process

/** OpenRouter's public model list (no key needed).  `fetchImpl` is for tests. */
async function loadCatalogue(fetchImpl = fetch) {
  if (!catalogue) {
    const res = await fetchImpl(MODELS_URL, { signal: AbortSignal.timeout(30_000) });
    if (!res.ok) throw new Error(`could not read OpenRouter prices (${res.status}); refusing to run a model without a price check`);
    const body = await res.json();
    if (!Array.isArray(body?.data)) throw new Error('unexpected OpenRouter model list; refusing to run a model without a price check');
    catalogue = body.data;
  }
  return catalogue;
}

/** Forget the cached model list (tests). */
export function resetCatalogue() {
  catalogue = undefined;
}

/**
 * Allowlist, then (for OpenRouter) the live price check.  Returns the canonical { provider, model }.
 * Throws (fail-closed) if the model is not in the pool, costs more than the ceiling, or its price cannot be read.
 */
export async function assertModelRunnable(provider, model, { fetchImpl } = {}) {
  const ok = assertAllowedModel(provider, model);
  if (provider !== 'openrouter') return ok; // Claude: subscription, bounded by usage limits and the per-run budget
  let list;
  try {
    list = await loadCatalogue(fetchImpl);
  } catch (e) {
    throw new Error(e instanceof Error && /refusing/.test(e.message) ? e.message : `could not read OpenRouter prices (${e instanceof Error ? e.message : e}); refusing to run a model without a price check`);
  }
  const entry = list.find((m) => m.id === ok.model);
  if (!entry) throw new Error(`model ${ok.model} is not in OpenRouter's list; refusing to run it`);
  checkPricing(entry);
  return ok;
}

/**
 * Worst-case cost of some token usage, at the ceiling prices.  Used to stop a run that goes over budget even when the
 * tool that runs the model reports no cost (new or aliased models often show as free in a local catalogue).
 * Every input-like token (including cache reads and writes) is priced as a prompt token; reasoning tokens as output.
 */
export function tokenCostUsd({ input = 0, output = 0, reasoning = 0, cacheRead = 0, cacheWrite = 0 } = {}) {
  return (input + cacheRead + cacheWrite) * PRICE_CEILING.prompt + (output + reasoning) * PRICE_CEILING.completion;
}
