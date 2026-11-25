import assert from 'node:assert/strict';
import { test } from 'node:test';

import { decideApproval } from '../autofix.mjs';
import { MIN_PARTICIPANTS, aggregate, reviewPrompt, reviewerConfig } from '../council-lib.mjs';
import { UI_LIMITS, classifyChange } from '../policy.mjs';

const f = (path, over = {}) => ({ path, status: 'modified', additions: 5, deletions: 2, ...over });
const AF = { autoFix: true };

// ------------------------------------------------------------------------------------------ who may approve

const labeled = (login, type = 'User', event = 'labeled') => ({ event, label: { name: 'auto-fix' }, actor: { login, type } });
const perm = (table) => (login) => table[login] ?? 'none';

test('auto-fix counts only when a person with write access applied it last', () => {
  const people = perm({ maya: 'write', root: 'admin', lead: 'maintain', reader: 'read' });
  assert.equal(decideApproval([labeled('maya')], ['auto-fix'], people).approved, true);
  assert.equal(decideApproval([labeled('root')], ['auto-fix'], people).approved, true);
  assert.equal(decideApproval([labeled('lead')], ['auto-fix'], people).approved, true);
  assert.equal(decideApproval([labeled('maya')], ['auto-fix'], people).by, 'maya');
  // without write access, or from a bot, or when the label is gone, or when nobody can be named: no
  assert.equal(decideApproval([labeled('reader')], ['auto-fix'], people).approved, false);
  assert.equal(decideApproval([labeled('stranger')], ['auto-fix'], people).approved, false);
  assert.equal(decideApproval([labeled('github-actions[bot]', 'Bot')], ['auto-fix'], people).approved, false);
  assert.equal(decideApproval([labeled('maya')], ['bug'], people).approved, false);
  assert.equal(decideApproval([], ['auto-fix'], people).approved, false);
  assert.equal(decideApproval([{ event: 'labeled', label: { name: 'auto-fix' } }], ['auto-fix'], people).approved, false);
  assert.equal(decideApproval(undefined, ['auto-fix'], people).approved, false);
});

test('the latest event decides: a label removed and re-applied by someone else is theirs', () => {
  const people = perm({ maya: 'write', reader: 'read' });
  const events = [labeled('maya'), labeled('maya', 'User', 'unlabeled'), labeled('reader')];
  assert.equal(decideApproval(events, ['auto-fix'], people).approved, false);
  assert.equal(decideApproval([labeled('reader'), labeled('maya')], ['auto-fix'], people).approved, true);
  // other labels do not matter
  const noise = [labeled('maya'), { event: 'labeled', label: { name: 'bug' }, actor: { login: 'reader', type: 'User' } }, { event: 'commented' }];
  assert.equal(decideApproval(noise, ['auto-fix', 'bug'], people).approved, true);
});

// ------------------------------------------------------------------------------------------ what it may change

test('without auto-fix nothing about the interface changes: it is still blocked, whatever else is true', () => {
  for (const [files, lines] of [
    [[f('glabels/ui/MainWindow.ui')], []],
    [[f('glabels/Style.qss')], []],
    [[f('glabels/Dialog.cpp')], ['label->setText( tr("Cancel") );']],
    [[f('glabels/Dialog.cpp')], ['layout->setContentsMargins( 4, 4, 4, 4 );']],
  ]) {
    const r = classifyChange(files, lines);
    assert.equal(r.risk, 'blocked', JSON.stringify(lines));
    assert.equal(r.ui, true);
  }
  assert.equal(classifyChange([f('glabels/A.cpp')], ['int x = 3;']).ui, false);
});

test('with auto-fix a small change to the existing interface is ordinary low-risk work', () => {
  const small = classifyChange([f('glabels/ui/PropertiesDialog.ui'), f('glabels/PropertiesDialog.cpp')], [
    '<string>Print all pages</string>', 'm_button->setText( tr("Print all pages") );', 'm_layout->setContentsMargins( 8, 8, 8, 8 );', 'setMinimumSize( 320, 200 );',
  ], AF);
  assert.equal(small.risk, 'low');
  assert.equal(small.ui, true);
  assert.equal(classifyChange([f('glabels/Style.qss', { additions: 4, deletions: 4 })], ['QPushButton { padding: 6px; }'], AF).risk, 'low');
});

test('with auto-fix, ADDING to the interface, new interface files and removing them stay with a person', () => {
  for (const line of [
    'layout->addWidget( new QPushButton( "Go" ) );', 'm_box = new QComboBox( this );', 'menu->addAction( act );', 'tabs->insertTab( 1, page, "x" );',
    '<widget class="QPushButton" name="goButton">', '<action name="actionGo">', '<layout class="QVBoxLayout" name="l">', '<spacer name="s">',
  ]) {
    assert.equal(classifyChange([f('glabels/ui/Main.ui')], [line], AF).risk, 'blocked', line);
  }
  assert.equal(classifyChange([f('glabels/ui/New.ui', { status: 'added' })], [], AF).risk, 'blocked');
  assert.equal(classifyChange([f('glabels/ui/Old.ui', { status: 'removed' })], [], AF).risk, 'blocked');
  assert.equal(classifyChange([f('glabels/ui/Old.ui', { status: 'renamed' })], [], AF).risk, 'blocked');
});

test('with auto-fix, the size limits: 100 lines and 3 files are small, up to 200 and 6 need every reviewer, beyond that a person', () => {
  const ui = (n, files = 1) => Array.from({ length: files }, (_, i) => f(`glabels/ui/F${i}.ui`, { additions: Math.ceil(n / files), deletions: 0 }));
  assert.equal(UI_LIMITS.softLines, 100);
  assert.equal(UI_LIMITS.softFiles, 3);
  assert.equal(classifyChange(ui(99, 3), [], AF).risk, 'low');
  assert.equal(classifyChange(ui(101, 1), [], AF).risk, 'high');
  assert.equal(classifyChange(ui(30, 4), [], AF).risk, 'high');
  assert.equal(classifyChange(ui(198, 6), [], AF).risk, 'high');
  assert.equal(classifyChange(ui(201, 1), [], AF).risk, 'blocked');
  assert.equal(classifyChange(ui(30, 7), [], AF).risk, 'blocked');
  // a change that does not touch the interface keeps the ordinary limits
  assert.equal(classifyChange([f('glabels/A.cpp', { additions: 130, deletions: 0 })], ['int a;'], AF).risk, 'high');
  assert.equal(classifyChange(['a', 'b', 'c', 'd'].map((n) => f(`glabels/${n}.cpp`)), ['int a;'], AF).risk, 'low');
});

test('auto-fix loosens only the interface rules: every other tripwire and path rule still blocks', () => {
  for (const [files, lines] of [
    [[f('glabels/ui/Main.ui'), f('CMakeLists.txt')], []],
    [[f('glabels/ui/Main.ui'), f('.github/workflows/ci.yml')], []],
    [[f('glabels/Dialog.cpp')], ['QProcess::startDetached( "calc" ); // setText( tr("x") )']],
    [[f('glabels/Dialog.cpp')], ['label->setText( "see https://example.com" );']],
    [[f('glabels/Dialog.cpp')], ['auto v = qgetenv( "HOME" );']],
    [[f('glabels/Dialog.cpp', { mode: '100755' })], ['label->setText( tr("x") );']],
    [[f('glabels/icon.png', { binary: true })], []],
    [[f('model/unit_tests/TestA.cpp', { status: 'removed' }), f('glabels/ui/Main.ui')], []],
  ]) assert.equal(classifyChange(files, lines, AF).risk, 'blocked', JSON.stringify([files.map((x) => x.path), lines]));
});

// ------------------------------------------------------------------------------------------ the council

const approve = { verdict: 'approve', risk: 'low', scope_ok: true, backdoor_suspected: false, concerns: [] };
const vote = (id, over = {}) => ({ id, verdict: { ...approve, ...over } });

test('an auto-fix change needs Opus: a required reviewer cannot be skipped, and its objection stops the merge', () => {
  assert.ok(MIN_PARTICIPANTS <= 2);
  const three = [vote('deepseek'), vote('glm'), vote('claude-haiku')];
  const expected = ['deepseek', 'glm', 'claude-haiku', 'claude-opus'];
  assert.equal(aggregate([...three, vote('claude-opus')], 'low', expected, ['claude-opus']).approved, true);
  const missing = aggregate(three, 'low', ['deepseek', 'glm', 'claude-haiku'], ['claude-opus']); // Opus not even asked (no secret)
  assert.equal(missing.approved, false);
  assert.match(missing.reasons.join(' '), /required reviewer claude-opus/);
  assert.equal(aggregate(three, 'low', expected, ['claude-opus']).approved, false); // asked, did not answer
  assert.equal(aggregate([...three, vote('claude-opus', { verdict: 'request_changes' })], 'low', expected, ['claude-opus']).approved, false);
  assert.equal(aggregate([...three, vote('claude-opus', { concerns: [{ severity: 'blocking', text: 'clips at 320px' }] })], 'low', expected, ['claude-opus']).approved, false);
  // without a required list nothing changes
  assert.equal(aggregate(three, 'low', ['deepseek', 'glm', 'claude-haiku']).approved, true);
});

test('the council gets Opus and a UI-minded prompt only for an auto-fix change', () => {
  assert.deepEqual(reviewerConfig().map((r) => r.id), ['deepseek', 'glm', 'claude-haiku']);
  assert.deepEqual(reviewerConfig({ autoFix: true }).map((r) => r.id), ['deepseek', 'glm', 'claude-haiku', 'claude-opus']);
  assert.equal(reviewerConfig({ autoFix: true }).at(-1).model, 'opus');
  const base = { title: 't', issueTitle: 'i', issueBody: 'b', summary: 's', diff: 'd', classification: { risk: 'low', reasons: [] } };
  assert.doesNotMatch(reviewPrompt(base), /USER-INTERFACE CHANGE/);
  assert.match(reviewPrompt({ ...base, autoFix: true }), /USER-INTERFACE CHANGE a maintainer explicitly approved/);
});
