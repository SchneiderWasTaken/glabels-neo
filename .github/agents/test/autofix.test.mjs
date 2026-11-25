import assert from 'node:assert/strict';
import { test } from 'node:test';

import { decideApproval } from '../autofix.mjs';
import { aggregate, reviewPrompt, reviewerConfig } from '../council-lib.mjs';
import { UI_LIMITS, classifyChange } from '../policy.mjs';

const f = (path, over = {}) => ({ path, status: 'modified', additions: 5, deletions: 2, ...over });
const AF = { autoFix: true };

// ------------------------------------------------------------------------------------------ who may approve

const T0 = '2026-11-25T10:00:00Z';
const labeled = (login, type = 'User', event = 'labeled', created_at = T0) => ({ event, created_at, label: { name: 'auto-fix' }, actor: { login, type } });
const perm = (table) => (login) => table[login] ?? 'none';

test('auto-fix counts only when a person with write access applied it last', () => {
  const people = perm({ maya: 'write', root: 'admin', lead: 'maintain', reader: 'read' });
  assert.equal(decideApproval([labeled('maya')], ['auto-fix'], people, null).approved, true);
  assert.equal(decideApproval([labeled('root')], ['auto-fix'], people, null).approved, true);
  assert.equal(decideApproval([labeled('lead')], ['auto-fix'], people, null).approved, true);
  assert.equal(decideApproval([labeled('maya')], ['auto-fix'], people, null).by, 'maya');
  // without write access, or from a bot, or when the label is gone, or when nobody can be named: no
  assert.equal(decideApproval([labeled('reader')], ['auto-fix'], people, null).approved, false);
  assert.equal(decideApproval([labeled('stranger')], ['auto-fix'], people, null).approved, false);
  assert.equal(decideApproval([labeled('github-actions[bot]', 'Bot')], ['auto-fix'], people, null).approved, false);
  assert.equal(decideApproval([labeled('maya')], ['bug'], people, null).approved, false);
  assert.equal(decideApproval([], ['auto-fix'], people, null).approved, false);
  assert.equal(decideApproval([{ event: 'labeled', created_at: T0, label: { name: 'auto-fix' } }], ['auto-fix'], people, null).approved, false);
  assert.equal(decideApproval(undefined, ['auto-fix'], people, null).approved, false);
});

test('approval is for the issue as it was: an edit or a rename after the label voids it, and unknown means no', () => {
  const people = perm({ maya: 'write' });
  const ok = (edited, extra = []) => decideApproval([labeled('maya'), ...extra], ['auto-fix'], people, edited);
  assert.equal(ok(null).approved, true);
  assert.equal(ok('2026-11-25T09:00:00Z').approved, true); // edited before the label: the maintainer saw this text
  assert.equal(ok('2026-11-25T10:00:01Z').approved, false);
  assert.match(ok('2026-11-25T11:00:00Z').why, /edited at or after/);
  assert.equal(ok(T0).approved, false); // the same second cannot be told apart from later
  assert.equal(ok('not a date').approved, false); // unreadable counts as later
  assert.equal(ok(null, [{ event: 'renamed', created_at: T0 }]).approved, false);
  assert.equal(ok(null, [{ event: 'renamed', created_at: 'garbage' }]).approved, false);
  assert.equal(ok(undefined).approved, false); // could not ask GitHub
  assert.equal(ok(null, [{ event: 'renamed', created_at: '2026-11-25T12:00:00Z' }]).approved, false);
  assert.equal(ok(null, [{ event: 'renamed', created_at: '2026-11-25T08:00:00Z' }]).approved, true);
  assert.equal(decideApproval([{ ...labeled('maya'), performed_via_github_app: { slug: 'x' } }], ['auto-fix'], people, null).approved, false);
  assert.equal(decideApproval([{ ...labeled('maya'), created_at: undefined }], ['auto-fix'], people, null).approved, false);
});

test('the latest event decides: a label removed and re-applied by someone else is theirs', () => {
  const people = perm({ maya: 'write', reader: 'read' });
  const events = [labeled('maya'), labeled('maya', 'User', 'unlabeled'), labeled('reader')];
  assert.equal(decideApproval(events, ['auto-fix'], people, null).approved, false);
  assert.equal(decideApproval([labeled('reader'), labeled('maya')], ['auto-fix'], people, null).approved, true);
  // other labels do not matter
  const noise = [labeled('maya'), { event: 'labeled', label: { name: 'bug' }, actor: { login: 'reader', type: 'User' } }, { event: 'commented' }];
  assert.equal(decideApproval(noise, ['auto-fix', 'bug'], people, null).approved, true);
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

test('with auto-fix the "no additions to the interface" rule cannot be dodged by layout, attribute order or indirection', () => {
  const ui = (lines) => classifyChange([f('glabels/ui/Main.ui')], lines, AF).risk;
  const cpp = (lines) => classifyChange([f('glabels/Dialog.cpp')], lines, AF).risk;
  // Qt Designer does not care about attribute order or line breaks
  assert.equal(ui(['<widget name="go" class="QPushButton">']), 'blocked');
  assert.equal(ui(['<widget', 'name="go" class="QPushButton">']), 'blocked');
  assert.equal(ui(['<addaction name="actionGo"/>']), 'blocked');
  assert.equal(ui(['<item><property name="text"><string>x</string></property></item>']), 'blocked');
  assert.equal(ui(['<connection><sender>a</sender><signal>clicked()</signal><receiver>b</receiver><slot>close()</slot></connection>']), 'blocked');
  assert.equal(ui(['<customwidget><class>Evil</class></customwidget>']), 'blocked');
  assert.equal(ui(['<include location="local">x.h</include>']), 'blocked');
  assert.equal(ui(['<header>x.h</header>']), 'blocked');
  // ...but properties of what exists are fine
  assert.equal(ui(['<property name="text"><string>Print all</string></property>', '<property name="minimumSize"><size><width>320</width><height>200</height></size></property>']), 'low');
  // calls split over lines, other classes and calls, factories, macros and aliases
  assert.equal(cpp(['layout->addWidget(', 'btn );']), 'blocked');
  assert.equal(cpp(['auto* b = new', 'QPushButton( this );']), 'blocked');
  assert.equal(cpp(['auto* b = new (std::nothrow) QPushButton( this );']), 'blocked');
  assert.equal(cpp(['auto b = std::make_unique<QPushButton>( this );']), 'blocked');
  assert.equal(cpp(['auto* l = new QVBoxLayout( this );']), 'blocked');
  assert.equal(cpp(['auto* w = new QFrame( this );']), 'blocked');
  assert.equal(cpp(['buttonBox->addButton( "Export", QDialogButtonBox::ActionRole );']), 'blocked');
  assert.equal(cpp(['bar->addSeparator();']), 'blocked');
  assert.equal(cpp(['setLayout( l );']), 'blocked');
  assert.equal(cpp(['QMessageBox::information( this, "t", "x" );']), 'blocked');
  assert.equal(cpp(['connect( a, &A::x, b, &B::close );']), 'blocked');
  assert.equal(cpp(['#define ADDW addWidget']), 'blocked');
  assert.equal(cpp(['using B = QPushButton;']), 'blocked');
  assert.equal(cpp(['typedef QPushButton B;']), 'blocked');
  // comments, the project's own widgets, stack widgets, static dialogs, macros split over lines
  assert.equal(cpp(['layout->addWidget/**/( w );']), 'blocked');
  assert.equal(cpp(['setLayout/* x */( l );']), 'blocked');
  assert.equal(cpp(['auto* b = new RibbonButton( this );']), 'blocked');
  assert.equal(cpp(['QDialog dlg( this );']), 'blocked');
  assert.equal(cpp(['QPushButton btn( tr("x"), this );']), 'blocked');
  assert.equal(cpp(['auto name = QFileDialog::getOpenFileName( this );']), 'blocked');
  assert.equal(cpp(['dlg.exec();']), 'blocked');
  assert.equal(cpp(['using', 'B = QPushButton;']), 'blocked');
  assert.equal(cpp(['#\\', 'define X addWidget']), 'blocked');
  // icons are part of the interface: changing one is fine, adding or removing one is not
  assert.equal(classifyChange([f('glabels/icons/a.svg', { status: 'added' })], [], AF).risk, 'blocked');
  assert.equal(classifyChange([f('glabels/icons/a.svg', { status: 'removed' })], [], AF).risk, 'blocked');
  assert.equal(classifyChange([f('glabels/icons/a.svg')], ['<path d="M0 0"/>'], AF).risk, 'low');
  // ordinary tweaks still pass
  assert.equal(cpp(['m_label->setText( tr("Print all pages") );', 'm_layout->setSpacing( 8 );']), 'low');
  // resource manifests are not wording or spacing
  assert.equal(classifyChange([f('glabels/icons.qrc')], ['<file>a.png</file>'], AF).risk, 'blocked');
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
  // the label is for SMALL changes, whatever they touch
  assert.equal(classifyChange([f('glabels/A.cpp', { additions: 90, deletions: 0 })], ['int a;'], AF).risk, 'low');
  assert.equal(classifyChange([f('glabels/A.cpp', { additions: 130, deletions: 0 })], ['int a;'], AF).risk, 'high');
  assert.equal(classifyChange([f('glabels/A.cpp', { additions: 250, deletions: 0 })], ['int a;'], AF).risk, 'blocked');
  assert.equal(classifyChange(['a', 'b', 'c', 'd'].map((n) => f(`glabels/${n}.cpp`)), ['int a;'], AF).risk, 'high');
  assert.equal(classifyChange(['a', 'b', 'c'].map((n) => f(`glabels/${n}.cpp`)), ['int a;'], AF).risk, 'low');
  // without the label the ordinary limits are unchanged
  assert.equal(classifyChange([f('glabels/A.cpp', { additions: 130, deletions: 0 })], ['int a;']).risk, 'high');
  assert.equal(classifyChange([f('glabels/A.cpp', { additions: 90, deletions: 0 })], ['int a;']).risk, 'low');
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
  assert.match(reviewPrompt({ ...base, autoFix: true }), /explicitly approved this issue with the "auto-fix" label/);
});
