// Council step 2: one reviewer reads the change and returns a structured verdict.
//   node council-review.mjs '<reviewer json>'
//
// Reviewers get the same prompt and no tools: they read text and answer in JSON.  The verdict is
// validated and scanned for secrets before it is written out.

import { assertNoSecrets, readJson, runClaude, secretsFromEnv, writeJson } from './lib.mjs';
import { assertModelRunnable } from './model-policy.mjs';
import { extractJson } from './opencode-lib.mjs';
import { VERDICT_SCHEMA, reviewPrompt, validateVerdict } from './council-lib.mjs';

const reviewer = JSON.parse(process.argv[2]);
const input = readJson('council-input.json');
const prompt = reviewPrompt(input);

async function viaClaude() {
  await assertModelRunnable('claude', reviewer.model);
  const r = await runClaude({
    prompt, model: reviewer.model, tools: '', schema: VERDICT_SCHEMA, maxBudgetUsd: 1.5,
    system: 'You are a strict, fair code reviewer. You have no tools. Output only the requested JSON.',
  });
  return r.structured;
}

async function viaOpenRouter() {
  // Allowlist and live price check before anything is sent.
  const { model: slug } = await assertModelRunnable('openrouter', reviewer.model);
  const res = await fetch('https://openrouter.ai/api/v1/chat/completions', {
    method: 'POST',
    headers: { authorization: `Bearer ${process.env.OPENROUTER_API_KEY}`, 'content-type': 'application/json' },
    body: JSON.stringify({
      model: slug,
      messages: [
        { role: 'system', content: 'You are a strict, fair code reviewer. Reply with ONE JSON object matching this JSON schema and nothing else: ' + JSON.stringify(VERDICT_SCHEMA) },
        { role: 'user', content: prompt },
      ],
      response_format: { type: 'json_object' },
      temperature: 0.1,
      max_tokens: 4000,
    }),
    signal: AbortSignal.timeout(10 * 60 * 1000),
  });
  if (!res.ok) throw new Error(`OpenRouter returned ${res.status}`);
  const data = await res.json();
  return extractJson(String(data.choices?.[0]?.message?.content ?? ''));
}

const providers = { claude: viaClaude, openrouter: viaOpenRouter };
const out = { id: reviewer.id, model: reviewer.model, sha: input.sha };

/** Two attempts: transient provider errors should not veto a good change (a missing answer blocks approval). */
async function withRetry(fn) {
  try {
    return await fn();
  } catch (first) {
    console.log(`retrying after: ${String(first instanceof Error ? first.message : first).slice(0, 120)}`);
    return fn();
  }
}

try {
  const verdict = validateVerdict(await withRetry(providers[reviewer.provider]));
  assertNoSecrets(JSON.stringify(verdict), secretsFromEnv('OPENROUTER_API_KEY', 'CLAUDE_CODE_OAUTH_TOKEN', 'GH_TOKEN'));
  out.verdict = verdict;
  console.log(`${reviewer.id}: ${verdict.verdict}, risk ${verdict.risk}`);
} catch (e) {
  // A reviewer that cannot answer simply does not take part; it can never count as an approval.
  out.error = String(e instanceof Error ? e.message : e).slice(0, 300);
  console.log(`${reviewer.id}: no verdict (${out.error})`);
}
writeJson(`verdict-${reviewer.id}.json`, out);
