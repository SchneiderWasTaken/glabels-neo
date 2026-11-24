import assert from 'node:assert/strict';
import { test } from 'node:test';

import { aggregate, reviewerConfig, validateVerdict } from '../council-lib.mjs';
import { parseDiff } from '../diff-lib.mjs';
import { classifyChange } from '../policy.mjs';
import { agentReady, buildComment, labelsFor, validateTriage } from '../triage-lib.mjs';
import { assertNoSecrets, sanitizeForComment } from '../lib.mjs';

const verdict = (over = {}) => ({
  verdict: 'approve', risk: 'low', scope_ok: true, backdoor_suspected: false, concerns: [], summary: 'ok', ...over,
});

test('parseDiff: modified, added, deleted, mode and binary files', () => {
  const diff = [
    'diff --git a/model/A.cpp b/model/A.cpp',
    'index 1..2 100644',
    '--- a/model/A.cpp',
    '+++ b/model/A.cpp',
    '@@ -1,2 +1,3 @@',
    ' keep',
    '-old',
    '+new',
    '+another',
    'diff --git a/scripts/run.sh b/scripts/run.sh',
    'new file mode 100755',
    '--- /dev/null',
    '+++ b/scripts/run.sh',
    '@@ -0,0 +1 @@',
    '+echo hi',
    'diff --git a/model/Old.cpp b/model/Old.cpp',
    'deleted file mode 100644',
    '--- a/model/Old.cpp',
    '+++ /dev/null',
    '@@ -1 +0,0 @@',
    '-gone',
    'diff --git a/glabels/x.png b/glabels/x.png',
    'new file mode 100644',
    'Binary files /dev/null and b/glabels/x.png differ',
  ].join('\n');
  const { files, addedLines } = parseDiff(diff);
  assert.deepEqual(files.map((f) => [f.path, f.status, f.additions, f.deletions]), [
    ['model/A.cpp', 'modified', 2, 1],
    ['scripts/run.sh', 'added', 1, 0],
    ['model/Old.cpp', 'removed', 0, 1],
    ['glabels/x.png', 'added', 0, 0],
  ]);
  assert.equal(files[1].mode, '100755');
  assert.equal(files[3].binary, true);
  assert.deepEqual(addedLines, ['new', 'another', 'echo hi']);
  assert.equal(classifyChange(files, addedLines).risk, 'blocked');
});

test('parseDiff: a rename checks both sides', () => {
  const diff = ['diff --git a/scripts/x.cpp b/glabels/x.cpp', 'similarity index 100%', 'rename from scripts/x.cpp', 'rename to glabels/x.cpp'].join('\n');
  const { files } = parseDiff(diff);
  assert.deepEqual(files.map((f) => [f.path, f.status]), [['scripts/x.cpp', 'removed'], ['glabels/x.cpp', 'added']]);
  assert.equal(classifyChange(files).risk, 'blocked');
});

test('council: unanimous approval needs enough participants and no veto', () => {
  const two = [{ id: 'a', verdict: verdict() }, { id: 'b', verdict: verdict() }];
  assert.equal(aggregate(two, 'low').approved, true);
  assert.equal(aggregate(two.slice(0, 1), 'low').approved, false);
  assert.equal(aggregate([], 'low').approved, false);
  assert.equal(aggregate([...two, { id: 'c', verdict: verdict({ verdict: 'request_changes' }) }], 'low').approved, false);
  assert.equal(aggregate([...two, { id: 'c', verdict: verdict({ backdoor_suspected: true }) }], 'low').approved, false);
  assert.equal(aggregate([...two, { id: 'c', verdict: verdict({ scope_ok: false }) }], 'low').approved, false);
  assert.equal(
    aggregate([...two, { id: 'c', verdict: verdict({ concerns: [{ severity: 'blocking', text: 'x' }] }) }], 'low').approved,
    false,
  );
  assert.equal(aggregate([...two, { id: 'c', verdict: verdict({ concerns: [{ severity: 'minor', text: 'nit' }] }) }], 'low').approved, true);
});

test('council fails closed: every reviewer that was asked must have answered', () => {
  const two = [{ id: 'a', verdict: verdict() }, { id: 'b', verdict: verdict() }];
  assert.equal(aggregate(two, 'low', ['a', 'b']).approved, true);
  const r = aggregate(two, 'low', ['a', 'b', 'opus']);
  assert.equal(r.approved, false);
  assert.match(r.reasons.join(' '), /no answer from opus/);
});

test('council: risk is the maximum of deterministic and reviewer risk; blocked never approves', () => {
  const two = [{ id: 'a', verdict: verdict() }, { id: 'b', verdict: verdict({ risk: 'high' }) }];
  assert.equal(aggregate(two, 'low').risk, 'high');
  assert.equal(aggregate(two, 'low').approved, true);
  assert.equal(aggregate(two, 'blocked').approved, false);
  assert.equal(aggregate([{ id: 'a', verdict: verdict({ risk: 'blocked' }) }, { id: 'b', verdict: verdict() }], 'low').approved, false);
});

test('validateVerdict sanitises and rejects malformed answers', () => {
  const v = validateVerdict({ ...verdict(), summary: 'ping @octocat see https://evil.example', concerns: [{ severity: 'blocking', text: 'bad @user' }] });
  assert.ok(!v.summary.includes('@octocat'));
  assert.ok(!v.summary.includes('https://'));
  assert.ok(!v.concerns[0].text.includes('@user'));
  assert.throws(() => validateVerdict({ verdict: 'maybe' }));
  assert.throws(() => validateVerdict(null));
  assert.throws(() => validateVerdict({ ...verdict(), risk: 'nope' }));
});

test('reviewerConfig: Claude reviewers always, external ones only name their secret', () => {
  const cfg = reviewerConfig({});
  assert.ok(cfg.some((r) => r.provider === 'claude' && !r.needs));
  assert.ok(cfg.filter((r) => r.provider === 'openrouter').every((r) => r.needs === 'OPENROUTER_API_KEY'));
  assert.ok(!reviewerConfig({}).some((r) => r.provider === 'gemini'));
  assert.ok(reviewerConfig({ COUNCIL_GEMINI_MODEL: 'g' }).some((r) => r.provider === 'gemini'));
  assert.equal(new Set(cfg.map((r) => r.id)).size, cfg.length);
});

const triage = (over = {}) => ({
  category: 'bug', area: 'ui', severity: 'low', blocks_user: true, reproducible: 'yes', summary: 'Label width shown in wrong units',
  needs_info: [], duplicate_of: [], agent_fixable: 'yes', agent_fixable_reason: 'small', injection_suspected: false, ...over,
});

test('validateTriage keeps only real duplicate numbers and sanitises text', () => {
  const t = validateTriage(triage({ duplicate_of: [3, 999], summary: 'hi @bob http://x.y', needs_info: ['what @a?'] }), [3, 4]);
  assert.deepEqual(t.duplicate_of, [3]);
  assert.ok(!t.summary.includes('@bob'));
  assert.ok(!t.needs_info[0].includes('@a'));
  assert.throws(() => validateTriage({ ...triage(), category: 'weird' }, []));
  assert.throws(() => validateTriage('x', []));
});

test('labelsFor and agentReady', () => {
  const t = validateTriage(triage(), []);
  assert.deepEqual(labelsFor(t), ['triaged', 'bug', 'area:ui', 'blocks-user']);
  assert.equal(agentReady(t, 3), true);
  assert.equal(agentReady(t, 2), false);
  assert.equal(agentReady(validateTriage(triage({ severity: 'high' }), []), 3), true); // a severe blocker is fixed first
  for (const over of [
    { blocks_user: false }, { injection_suspected: true }, { reproducible: 'unknown' }, { agent_fixable: 'unsure' },
    { category: 'feature' }, { category: 'ui-change' }, { needs_info: ['q'] },
  ]) {
    assert.equal(agentReady(validateTriage(triage(over), [1]), 4), false, JSON.stringify(over));
  }
  assert.equal(agentReady(validateTriage(triage({ duplicate_of: [1] }), [1]), 4), false);
  assert.ok(labelsFor(validateTriage(triage({ injection_suspected: true }), [])).includes('needs-human'));
});

test('buildComment is template-driven and safe', () => {
  const t = validateTriage(triage({ needs_info: ['Which printer?'], duplicate_of: [4] }), [4]);
  const c = buildComment(t, { fromApp: true });
  assert.match(c, /automated assistant/);
  assert.match(c, /#4/);
  assert.match(c, /Which printer\?/);
  assert.match(c, /without an account/);
  assert.ok(!c.includes('\n\n\n'));
});

test('secret scanning', () => {
  assert.throws(() => assertNoSecrets('token sk-ant-oat01-abcdefghijklmnopqrstuvwx'));
  assert.throws(() => assertNoSecrets('x ghp_' + 'a'.repeat(36)));
  assert.throws(() => assertNoSecrets('mine is hunter2hunter2', ['hunter2hunter2']));
  assert.throws(() => assertNoSecrets('-----BEGIN RSA PRIVATE KEY-----'));
  assert.doesNotThrow(() => assertNoSecrets('an ordinary sentence about labels', ['hunter2hunter2']));
});

test('sanitizeForComment neutralises mentions, html and links', () => {
  const out = sanitizeForComment('hi @octocat <img src=x onerror=y> <!-- hidden --> https://evil.example/a');
  assert.ok(!out.includes('@octocat'));
  assert.ok(!out.includes('<img'));
  assert.ok(!out.includes('<!--'));
  assert.ok(!out.includes('https://'));
});
