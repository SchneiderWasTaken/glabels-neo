// Council step 2: one reviewer reads the change and returns a structured verdict.
//   node council-review.mjs '<reviewer json>'
//
// Every provider gets the same prompt and no tools: reviewers read text and answer in JSON.  The verdict is
// validated and scanned for secrets before it is written out.

import { assertNoSecrets, readJson, runClaude, secretsFromEnv, writeJson } from './lib.mjs';
import { VERDICT_SCHEMA, reviewPrompt, validateVerdict } from './council-lib.mjs';

const reviewer = JSON.parse(process.argv[2]);
const input = readJson('council-input.json');
const prompt = reviewPrompt(input);

async function viaClaude() {
  const r = await runClaude({
    prompt, model: reviewer.model, tools: '', schema: VERDICT_SCHEMA, maxBudgetUsd: 1.5,
    system: 'You are a strict, fair code reviewer. You have no tools. Output only the requested JSON.',
  });
  return r.structured;
}

function extractJson(text) {
  const start = text.indexOf('{');
  const end = text.lastIndexOf('}');
  if (start < 0 || end <= start) throw new Error('no JSON object in the reply');
  return JSON.parse(text.slice(start, end + 1));
}

async function viaOpenRouter() {
  const res = await fetch('https://openrouter.ai/api/v1/chat/completions', {
    method: 'POST',
    headers: { authorization: `Bearer ${process.env.OPENROUTER_API_KEY}`, 'content-type': 'application/json' },
    body: JSON.stringify({
      model: reviewer.model,
      messages: [
        { role: 'system', content: 'You are a strict, fair code reviewer. Reply with ONE JSON object matching this JSON schema and nothing else: ' + JSON.stringify(VERDICT_SCHEMA) },
        { role: 'user', content: prompt },
      ],
      response_format: { type: 'json_object' },
      temperature: 0.1,
    }),
    signal: AbortSignal.timeout(10 * 60 * 1000),
  });
  if (!res.ok) throw new Error(`OpenRouter returned ${res.status}`);
  const data = await res.json();
  return extractJson(String(data.choices?.[0]?.message?.content ?? ''));
}

async function viaGemini() {
  const res = await fetch(`https://generativelanguage.googleapis.com/v1beta/models/${encodeURIComponent(reviewer.model)}:generateContent`, {
    method: 'POST',
    headers: { 'x-goog-api-key': process.env.GEMINI_API_KEY, 'content-type': 'application/json' },
    body: JSON.stringify({
      contents: [{ role: 'user', parts: [{ text: prompt + '\n\nReply with ONE JSON object matching this JSON schema and nothing else: ' + JSON.stringify(VERDICT_SCHEMA) }] }],
      generationConfig: { responseMimeType: 'application/json', temperature: 0.1 },
    }),
    signal: AbortSignal.timeout(10 * 60 * 1000),
  });
  if (!res.ok) throw new Error(`Gemini returned ${res.status}`);
  const data = await res.json();
  return extractJson(String(data.candidates?.[0]?.content?.parts?.map((x) => x.text).join('') ?? ''));
}

const providers = { claude: viaClaude, openrouter: viaOpenRouter, gemini: viaGemini };
const out = { id: reviewer.id, model: reviewer.model, sha: input.sha };

try {
  const verdict = validateVerdict(await providers[reviewer.provider]());
  assertNoSecrets(JSON.stringify(verdict), secretsFromEnv('CLAUDE_CODE_OAUTH_TOKEN', 'OPENROUTER_API_KEY', 'GEMINI_API_KEY', 'GH_TOKEN'));
  out.verdict = verdict;
  console.log(`${reviewer.id}: ${verdict.verdict}, risk ${verdict.risk}`);
} catch (e) {
  // A reviewer that cannot answer simply does not take part; it can never count as an approval.
  out.error = String(e instanceof Error ? e.message : e).slice(0, 300);
  console.log(`${reviewer.id}: no verdict (${out.error})`);
}
writeJson(`verdict-${reviewer.id}.json`, out);
