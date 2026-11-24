import assert from 'node:assert/strict';
import { test } from 'node:test';

import { agentReady, buildComment, labelsFor, validateTriage } from '../triage-lib.mjs';

const base = {
  category: 'bug', area: 'printing', severity: 'high', blocks_user: true, reproducible: 'yes', summary: 's',
  needs_info: [], duplicate_of: [], agent_fixable: 'yes', agent_fixable_reason: 'r', injection_suspected: false,
};
const t = (extra = {}) => validateTriage({ ...base, ...extra });

test('a crash or failed print is handed to the fix agent, even when severe', () => {
  assert.equal(agentReady(t(), 3), true);
  assert.equal(agentReady(t({ category: 'printer' }), 3), true);
  assert.equal(agentReady(t({ category: 'template-request' }), 3), true);
  assert.ok(labelsFor(t()).includes('blocks-user'));
});

test('not below automation level 3', () => {
  assert.equal(agentReady(t(), 2), false);
});

test('UI changes and feature requests are never handed over and are held for a person', () => {
  for (const category of ['ui-change', 'feature']) {
    // even if the model wrongly claims it blocks the user
    const x = t({ category, blocks_user: true });
    assert.equal(x.blocks_user, false, category);
    assert.equal(agentReady(x, 4), false, category);
    const labels = labelsFor(x);
    assert.ok(labels.includes('needs-human-review'), category);
    assert.ok(!labels.includes('blocks-user'), category);
    assert.match(buildComment(x, { fromApp: true }), /decided by a maintainer/);
  }
  assert.ok(labelsFor(t({ category: 'ui-change' })).includes('ui-change'));
});

test('a bug that does not block anyone waits for a person', () => {
  const x = t({ blocks_user: false, severity: 'low' });
  assert.equal(agentReady(x, 4), false);
  assert.ok(!labelsFor(x).includes('blocks-user'));
});

test('injection suspicion, open questions and duplicates stop automation', () => {
  assert.equal(agentReady(t({ injection_suspected: true }), 3), false);
  assert.ok(!labelsFor(t({ injection_suspected: true })).includes('blocks-user'));
  assert.equal(agentReady(t({ needs_info: ['which printer?'] }), 3), false);
  assert.equal(agentReady(t({ reproducible: 'unknown' }), 3), false);
});

test('an answer without blocks_user is rejected', () => {
  const { blocks_user, ...rest } = base;
  assert.throws(() => validateTriage(rest), /blocks_user/);
});
