// Pure helpers for running models through opencode (https://opencode.ai) instead of the Claude Code CLI.
//
// opencode lets the automation use cheap OpenRouter models (see model-policy.mjs for the only ones allowed).  Everything
// that decides what the model may do lives here, as plain data, so it can be unit-tested without a model:
//   - buildConfig():  the opencode configuration (inline JSON) for a run, with every capability off unless needed
//   - parseEvents():  turns opencode's `--format json` event stream into { text, cost, toolCalls, error }
//   - extractJson():  pulls the JSON object out of a model's text answer (opencode has no schema-enforced output)

/** Built-in opencode tools.  All are disabled unless a mode switches some back on. */
const ALL_TOOLS = [
  'bash', 'read', 'glob', 'grep', 'list', 'edit', 'write', 'patch', 'webfetch', 'websearch', 'codesearch', 'task',
  'todowrite', 'todoread', 'skill', 'lsp', 'multiedit', 'apply_patch', 'question',
];

/** What each mode may use.  'none' = the model only reads its prompt; 'edit' = read and edit files in the work dir. */
const MODE_TOOLS = {
  none: [],
  edit: ['read', 'glob', 'grep', 'list', 'edit', 'write', 'patch', 'multiedit', 'apply_patch'],
};

/** "openrouter/deepseek/deepseek-v4.1-flash-1017" -> { provider: 'openrouter', slug: 'deepseek/deepseek-v4.1-flash-1017' } */
export function splitModel(model) {
  const s = String(model ?? '');
  const i = s.indexOf('/');
  if (i <= 0 || i === s.length - 1) throw new Error(`opencode model must look like provider/model, got "${s}"`);
  return { provider: s.slice(0, i), slug: s.slice(i + 1) };
}

/**
 * The opencode configuration for one run.  It is passed inline (OPENCODE_CONFIG_CONTENT), so nothing on the runner's
 * disk can widen it.  Capabilities are denied twice, as disabled tools and as permissions, and only the one provider
 * the run needs is enabled.  No plugins, MCP servers, LSPs, formatters, sharing, snapshots or auto-update.
 */
export function buildConfig({ model, mode = 'none' }) {
  if (!(mode in MODE_TOOLS)) throw new Error(`unknown opencode mode: ${mode}`);
  const { provider, slug } = splitModel(model);
  const on = new Set(MODE_TOOLS[mode]);
  const tools = Object.fromEntries(ALL_TOOLS.map((t) => [t, on.has(t)]));
  const edit = mode === 'edit' ? 'allow' : 'deny';
  const read = mode === 'edit' ? 'allow' : 'deny';
  const config = {
    $schema: 'https://opencode.ai/config.json',
    autoupdate: false,
    share: 'disabled',
    snapshot: false,
    model,
    enabled_providers: [provider],
    plugin: [],
    mcp: {},
    lsp: false,
    formatter: false,
    instructions: [],
    tools,
    permission: {
      bash: 'deny',
      webfetch: 'deny',
      websearch: 'deny',
      task: 'deny',
      skill: 'deny',
      external_directory: 'deny',
      doom_loop: 'deny',
      read,
      edit,
    },
  };
  // OpenRouter model ids that opencode's catalogue does not list yet (new models, ~latest aliases) still work.
  if (provider === 'openrouter') config.provider = { openrouter: { models: { [slug]: {} } } };
  return config;
}

import { tokenCostUsd } from './model-policy.mjs';

/**
 * Parse `opencode run --format json` output: one JSON event per line.  Returns the model's final text, the cost it
 * reports, the tokens it used, the tool calls it made and any error.  Unparseable lines (logs, banners) are ignored.
 */
export function parseEvents(stdout) {
  let text = '';
  let cost = 0;
  const tokens = { input: 0, output: 0, reasoning: 0, cacheRead: 0, cacheWrite: 0 };
  let error = null;
  const toolCalls = [];
  for (const line of String(stdout ?? '').split('\n')) {
    const t = line.trim();
    if (!t.startsWith('{')) continue;
    let ev;
    try {
      ev = JSON.parse(t);
    } catch {
      continue;
    }
    const part = ev.part ?? {};
    if (ev.type === 'text' && typeof part.text === 'string') text += (text ? '\n' : '') + part.text;
    else if (ev.type === 'step_finish') {
      cost += Number(part.cost) || 0;
      const k = part.tokens ?? {};
      tokens.input += Number(k.input) || 0;
      tokens.output += Number(k.output) || 0;
      tokens.reasoning += Number(k.reasoning) || 0;
      tokens.cacheRead += Number(k.cache?.read) || 0;
      tokens.cacheWrite += Number(k.cache?.write) || 0;
    }
    else if (ev.type === 'tool_use') toolCalls.push({ tool: part.tool ?? 'unknown', status: part.state?.status ?? '' });
    else if (ev.type === 'error') error = String(ev.error?.data?.message ?? ev.error?.message ?? ev.error?.name ?? 'unknown error');
  }
  return { text: text.trim(), cost, tokens, toolCalls, error };
}

/**
 * Running cost of an event stream so far, used to stop a run that exceeds its budget.  It is the larger of the cost the
 * tool reports and the tokens priced at the ceiling (model-policy.mjs), because a new or aliased model can be reported as
 * free.
 */
export function costSoFar(stdout) {
  const r = parseEvents(stdout);
  return Math.max(r.cost, tokenCostUsd(r.tokens));
}

/**
 * The first balanced JSON object in a model's answer.  Models wrap answers in prose or code fences even when told
 * not to; the caller still validates the result against its schema, so being lenient here is safe.
 */
export function extractJson(text) {
  const s = String(text ?? '');
  for (let start = s.indexOf('{'); start !== -1; start = s.indexOf('{', start + 1)) {
    let depth = 0;
    let inString = false;
    let escaped = false;
    for (let i = start; i < s.length; i++) {
      const c = s[i];
      if (inString) {
        if (escaped) escaped = false;
        else if (c === '\\') escaped = true;
        else if (c === '"') inString = false;
        continue;
      }
      if (c === '"') inString = true;
      else if (c === '{') depth++;
      else if (c === '}' && --depth === 0) {
        try {
          const v = JSON.parse(s.slice(start, i + 1));
          if (v && typeof v === 'object' && !Array.isArray(v)) return v;
        } catch {
          /* not JSON; try the next opening brace */
        }
        break;
      }
    }
  }
  return null;
}

/** Instruction appended to a prompt when the answer must be a JSON object of a given shape. */
export function schemaInstruction(schema) {
  return [
    '',
    'Your final answer must be ONE JSON object and nothing else (no prose, no code fence), matching this JSON schema:',
    JSON.stringify(schema),
  ].join('\n');
}
