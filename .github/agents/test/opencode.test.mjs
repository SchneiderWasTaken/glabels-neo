import assert from 'node:assert/strict';
import { test } from 'node:test';

import { buildConfig, costSoFar, extractJson, parseEvents, schemaInstruction, splitModel } from '../opencode-lib.mjs';

const ev = (o) => JSON.stringify(o);

test('splitModel', () => {
  assert.deepEqual(splitModel('openrouter/z-ai/glm-5.3-flash'), { provider: 'openrouter', slug: 'z-ai/glm-5.3-flash' });
  assert.throws(() => splitModel('glm'), /provider\/model/);
  assert.throws(() => splitModel('/x'), /provider\/model/);
});

test('a tool-less run has every tool and permission off, and only the one provider', () => {
  const c = buildConfig({ model: 'openrouter/z-ai/glm-5.3-flash', mode: 'none' });
  assert.ok(Object.values(c.tools).every((v) => v === false));
  assert.equal(c.permission.edit, 'deny');
  assert.equal(c.permission.read, 'deny');
  assert.equal(c.permission.bash, 'deny');
  assert.equal(c.permission.webfetch, 'deny');
  assert.equal(c.permission.external_directory, 'deny');
  assert.deepEqual(c.enabled_providers, ['openrouter']);
  assert.deepEqual(c.provider.openrouter.models, { 'z-ai/glm-5.3-flash': {} });
  assert.equal(c.share, 'disabled');
  assert.equal(c.autoupdate, false);
  assert.deepEqual(c.plugin, []);
  assert.deepEqual(c.mcp, {});
});

test('an edit run may read and edit files, and nothing that executes or reaches the network', () => {
  const c = buildConfig({ model: 'openrouter/deepseek/deepseek-v4.1-flash-1017', mode: 'edit' });
  for (const t of ['read', 'glob', 'grep', 'edit', 'write']) assert.equal(c.tools[t], true, t);
  for (const t of ['bash', 'webfetch', 'websearch', 'task', 'skill', 'lsp']) assert.equal(c.tools[t], false, t);
  assert.equal(c.permission.edit, 'allow');
  assert.equal(c.permission.bash, 'deny');
  assert.equal(c.permission.webfetch, 'deny');
  assert.equal(c.permission.external_directory, 'deny');
  assert.throws(() => buildConfig({ model: 'openrouter/x/y', mode: 'root' }), /unknown opencode mode/);
});

test('parseEvents reads text, cost, tokens and tool calls, and ignores noise', () => {
  const out = [
    'some banner',
    ev({ type: 'step_start', part: {} }),
    ev({ type: 'text', part: { text: 'first' } }),
    ev({ type: 'tool_use', part: { tool: 'edit', state: { status: 'completed' } } }),
    ev({ type: 'step_finish', part: { cost: 0.001, tokens: { input: 100, output: 20, reasoning: 5, cache: { read: 50, write: 10 } } } }),
    ev({ type: 'text', part: { text: 'second' } }),
    ev({ type: 'step_finish', part: { cost: 0.002, tokens: { input: 10, output: 2 } } }),
    '{not json',
  ].join('\n');
  const r = parseEvents(out);
  assert.equal(r.text, 'first\nsecond');
  assert.ok(Math.abs(r.cost - 0.003) < 1e-12);
  assert.deepEqual(r.tokens, { input: 110, output: 22, reasoning: 5, cacheRead: 50, cacheWrite: 10 });
  assert.deepEqual(r.toolCalls, [{ tool: 'edit', status: 'completed' }]);
  assert.equal(r.error, null);
  assert.equal(parseEvents(ev({ type: 'error', error: { data: { message: 'boom' } } })).error, 'boom');
});

test('the budget guard counts tokens even when the tool reports the run as free', () => {
  const free = ev({ type: 'step_finish', part: { cost: 0, tokens: { input: 2_000_000, output: 0 } } });
  assert.ok(costSoFar(free) > 0.2); // 2M input tokens at the ceiling is 0.30 USD
  assert.equal(costSoFar(''), 0);
  const reported = ev({ type: 'step_finish', part: { cost: 5, tokens: { input: 1 } } });
  assert.equal(costSoFar(reported), 5);
});

test('extractJson finds the object in prose and code fences, and survives braces in strings', () => {
  assert.deepEqual(extractJson('{"a":1}'), { a: 1 });
  assert.deepEqual(extractJson('Sure!\n```json\n{"a":{"b":[1,2]},"c":"}{"}\n```\nDone.'), { a: { b: [1, 2] }, c: '}{' });
  assert.deepEqual(extractJson('{oops} then {"ok":true}'), { ok: true });
  assert.equal(extractJson('no json here'), null);
  assert.equal(extractJson('[1,2,3]'), null);
  assert.equal(extractJson(''), null);
});

test('schemaInstruction embeds the schema', () => {
  assert.match(schemaInstruction({ type: 'object', required: ['x'] }), /"required":\["x"\]/);
});
