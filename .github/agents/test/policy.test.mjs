import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  addedLinesOf, agentMayChange, chicago, classifyChange, isShippable, maxRisk, mayAutoMerge, nextVersion, parseLevel,
  releaseDecision, retryableRun,
} from '../policy.mjs';

const f = (path, extra = {}) => ({ path, status: 'modified', additions: 5, deletions: 2, ...extra });

test('parseLevel clamps and defaults', () => {
  assert.equal(parseLevel(undefined), 1);
  assert.equal(parseLevel(''), 1);
  assert.equal(parseLevel('abc'), 1);
  assert.equal(parseLevel('-2'), 1);
  assert.equal(parseLevel('0'), 0);
  assert.equal(parseLevel('3'), 3);
  assert.equal(parseLevel('99'), 4);
});

test('agents may change source but never build, automation or packaging files', () => {
  for (const p of ['glabels/MainWindow.cpp', 'model/Frame.cpp', 'templates/avery-us-templates.xml', 'docs/FAQ.md', 'translations/glabels_de.ts']) {
    assert.equal(agentMayChange(p), true, p);
  }
  for (const p of [
    '.github/workflows/ci.yml', 'relay/src/index.js', 'scripts/package-linux.sh', 'CMakeLists.txt', 'glabels/CMakeLists.txt',
    'cmake/Foo.cmake', 'tools/release-train-trigger/x.ps1', 'LICENSE', 'THIRD-PARTY-NOTICES.md', 'data/foo.desktop', 'package.json',
    'model/Version.hpp.in', '.gitattributes', 'README.md', 'brand/logo.svg',
  ]) {
    assert.equal(agentMayChange(p), false, p);
  }
});

test('shippable paths', () => {
  assert.equal(isShippable('model/Frame.cpp'), true);
  assert.equal(isShippable('templates/x.xml'), true);
  assert.equal(isShippable('scripts/package-linux.sh'), true); // changes the package, so it ships
  for (const p of ['docs/a.md', 'README.md', '.github/workflows/x.yml', 'brand/a.svg', 'tools/a.ps1', 'relay/src/a.js']) {
    assert.equal(isShippable(p), false, p);
  }
});

test('small ordinary fix is low risk', () => {
  const r = classifyChange([f('glabels/ObjectEditor.cpp'), f('model/unit_tests/TestFoo.cpp', { status: 'added' })], ['int x = 3;']);
  assert.deepEqual(r, { risk: 'low', reasons: [], ui: false });
});

test('template data fixes are low risk', () => {
  assert.equal(classifyChange([f('templates/avery-us-templates.xml')], ['<Label-rectangle id="x"/>']).risk, 'low');
});

test('sensitive areas and size make a change high risk', () => {
  assert.equal(classifyChange([f('model/XmlLabelParser.cpp')]).risk, 'high');
  assert.equal(classifyChange([f('model/PageRenderer.cpp')]).risk, 'high');
  assert.equal(classifyChange([f('glabels/ZplRenderer.cpp')]).risk, 'high');
  assert.equal(classifyChange([f('glabels/A.cpp', { additions: 150, deletions: 0 })]).risk, 'high');
  assert.equal(classifyChange(['a', 'b', 'c', 'd', 'e', 'f'].map((n) => f(`glabels/${n}.cpp`))).risk, 'high');
  assert.equal(classifyChange([f('glabels/A.cpp', { status: 'removed' })]).risk, 'high');
});

test('out-of-scope, oversized, binary and exec-bit changes are blocked', () => {
  assert.equal(classifyChange([f('.github/workflows/ci.yml')]).risk, 'blocked');
  assert.equal(classifyChange([f('CMakeLists.txt')]).risk, 'blocked');
  assert.equal(classifyChange([f('glabels/a.png', { binary: true })]).risk, 'blocked');
  assert.equal(classifyChange([f('glabels/run.cpp', { mode: '100755' })]).risk, 'blocked');
  assert.equal(classifyChange([f('glabels/A.cpp', { additions: 700, deletions: 0 })]).risk, 'blocked');
  assert.equal(classifyChange(Array.from({ length: 13 }, (_, i) => f(`glabels/f${i}.cpp`))).risk, 'blocked');
  assert.equal(classifyChange([]).risk, 'blocked');
  assert.equal(classifyChange([f('model/unit_tests/TestA.cpp', { status: 'removed' })]).risk, 'blocked');
});

test('tripwires block process execution, networking, env access, URLs and encoded blobs', () => {
  const bad = [
    'QProcess p; p.start("x");',
    'std::system("curl x");',
    'QNetworkAccessManager nam;',
    'auto v = qEnvironmentVariable("HOME");',
    '// see https://example.com/x',
    'const char* k = "' + 'AbCdEfGhIjKlMnOpQrStUvWxYz0123456789+/AbCdEfGhIj' + '";',
    'QLibrary lib("x");',
  ];
  for (const line of bad) {
    assert.equal(classifyChange([f('glabels/A.cpp')], [line]).risk, 'blocked', line);
  }
});

test('ordinary code and ruler comments do not trip', () => {
  const ok = [
    'for ( int i = 0; i < n; i++ ) { total += values[i]; }',
    '//' + '-'.repeat(70),
    '// ' + '='.repeat(70),
    'connect( button, &QPushButton::clicked, this, &Dialog::onClicked );',
    'QString name = QString::number( width );',
  ];
  for (const line of ok) {
    assert.equal(classifyChange([f('glabels/A.cpp')], [line]).risk, 'low', line);
  }
});

test('UI changes always need a human, whatever the issue said', () => {
  const blockedLines = [
    'QString name = tr( "Label width" );',
    'setWindowTitle( tr( "Print" ) );',
    'button->setToolTip( "Prints the sheet" );',
    'label->setText( "Width:" );',
    'setStyleSheet( "color: red" );',
    'layout->addWidget( new QPushButton( this ) );',
    'setMinimumSize( 400, 300 );',
  ];
  for (const line of blockedLines) {
    const r = classifyChange([f('glabels/A.cpp')], [line]);
    assert.equal(r.risk, 'blocked', line);
    assert.match(r.reasons.join(' '), /user-interface change/, line);
  }
  // a Designer form or stylesheet is a UI change on its own
  for (const p of ['glabels/ui/PrintView.ui', 'glabels/style.qss', 'glabels/icons.qrc']) {
    assert.equal(classifyChange([f(p)], ['x']).risk, 'blocked', p);
  }
  // an ordinary crash fix is not
  assert.equal(classifyChange([f('glabels/ObjectEditor.cpp')], ['if ( !mObject ) { return; }']).risk, 'low');
  assert.equal(classifyChange([f('model/Frame.cpp')], ['double w = qMax( 0.0, width );']).risk, 'low');
});

test('addedLinesOf extracts added lines only', () => {
  const diff = ['diff --git a/x b/x', '--- a/x', '+++ b/x', '@@ -1,2 +1,3 @@', ' ctx', '-old', '+new', '++plus'].join('\n');
  assert.deepEqual(addedLinesOf(diff), ['new', '+plus']);
});

test('a content line that starts with "++" cannot hide a tripwire', () => {
  const diff = ['diff --git a/glabels/A.cpp b/glabels/A.cpp', '--- a/glabels/A.cpp', '+++ b/glabels/A.cpp', '@@ -1 +1,2 @@', ' int x;', '+++i; QProcess::startDetached("x");'].join('\n');
  assert.equal(addedLinesOf(diff).length, 1);
  assert.equal(classifyChange([f('glabels/A.cpp')], addedLinesOf(diff)).risk, 'blocked');
});

test('tripwires also see tokens split across lines and string literals', () => {
  const split = ['QProc\\', 'ess p;'];
  assert.equal(classifyChange([f('glabels/A.cpp')], split).risk, 'blocked');
  assert.equal(classifyChange([f('glabels/A.cpp')], ['const char* u = "ht" "tps://example.com";']).risk, 'blocked');
  for (const line of ['QProcessEnvironment e;', 'QHostInfo::lookupHost(a, b);', 'posix_spawn(&p, a, 0, 0, 0, 0);', 'QFile f("/proc/self/environ");', '#define J(a,b) a##b']) {
    assert.equal(classifyChange([f('glabels/A.cpp')], [line]).risk, 'blocked', line);
  }
});

test('oversized diffs and test-weakening edits are blocked; sensitive code and scripts are out of scope', () => {
  assert.equal(classifyChange([f('glabels/A.cpp')], [], { diffChars: 60001 }).risk, 'blocked');
  assert.equal(classifyChange([f('glabels/A.cpp')], [], { diffChars: 59000 }).risk, 'low');
  assert.equal(classifyChange([f('model/unit_tests/TestA.cpp', { additions: 0, deletions: 30 })]).risk, 'blocked');
  assert.equal(classifyChange([f('model/unit_tests/TestA.cpp', { status: 'added', additions: 30, deletions: 0 })]).risk, 'low');
  for (const p of ['user-docs/conf.py', 'glabels/ReportBugDialog.cpp', 'model/BugReport.cpp', 'glabels/AboutDialog.cpp', 'scripts/X.SH', 'glabels/CMAKELISTS.TXT', 'glabels/a.PS1']) {
    assert.equal(agentMayChange(p), false, p);
  }
  for (const p of ['update/UpdateService.cpp', 'updater/glabels-updater.cpp', 'update/unit_tests/TestUpdateApply.cpp']) {
    assert.equal(agentMayChange(p), false, p);
  }
  assert.equal(agentMayChange('translations/glabels_de.ts'), true);
  assert.equal(agentMayChange('glabels/ui/PrintView.ui'), true);
});

test('maxRisk', () => {
  assert.equal(maxRisk('low', 'low'), 'low');
  assert.equal(maxRisk('low', 'high'), 'high');
  assert.equal(maxRisk('blocked', 'high', 'low'), 'blocked');
});

test('chicago time handles daylight saving', () => {
  assert.deepEqual(chicago(new Date('2026-07-01T15:00:00Z')), { date: '2026-07-01', hour: 10, minute: 0 }); // CDT
  assert.deepEqual(chicago(new Date('2026-12-01T16:00:00Z')), { date: '2026-12-01', hour: 10, minute: 0 }); // CST
  assert.deepEqual(chicago(new Date('2026-12-02T05:59:00Z')), { date: '2026-12-01', hour: 23, minute: 59 });
});

test('auto-merge rules by risk, level and time', () => {
  const morning = new Date('2026-12-01T13:00:00Z'); // 07:00 CST
  const afternoon = new Date('2026-12-01T20:30:00Z'); // 14:30 CST
  assert.equal(mayAutoMerge('blocked', 4, morning).ok, false);
  assert.equal(mayAutoMerge('low', 2, morning).ok, false);
  assert.equal(mayAutoMerge('low', 3, afternoon).ok, true);
  assert.equal(mayAutoMerge('high', 3, morning).ok, false);
  assert.equal(mayAutoMerge('high', 4, morning).ok, true);
  assert.equal(mayAutoMerge('high', 4, afternoon).ok, false);
  assert.equal(mayAutoMerge('high', 4, new Date('2026-12-01T15:30:00Z')).ok, false); // 09:30, too late to soak
});

const run = (iso) => ({ created_at: iso, head_branch: 'v1.2.1', event: 'workflow_dispatch' });

test('release train: inside the 10:00 / 14:00 windows only, with changes, once per slot, twice per day', () => {
  const base = { trainRuns: [], shippableChanged: true, headBuilt: true };
  const at = (iso, extra = {}) => releaseDecision({ ...base, ...extra, now: new Date(iso) });

  assert.equal(at('2026-12-01T16:05:00Z').release, true); // 10:05 CST
  assert.equal(at('2026-12-01T17:20:00Z').release, true); // 11:20 CST: a late scheduler is still inside the window
  assert.equal(at('2026-12-01T20:30:00Z').release, true); // 14:30 CST
  assert.equal(at('2026-12-01T17:40:00Z').release, false); // 11:40
  assert.equal(at('2026-12-01T15:00:00Z').release, false); // 09:00 (the CDT-hour cron during CST)
  assert.equal(at('2026-12-01T21:45:00Z').release, false); // 15:45
  assert.equal(at('2026-07-01T15:05:00Z').release, true); // 10:05 CDT

  assert.match(at('2026-12-01T16:05:00Z', { shippableChanged: false }).reason, /no user-visible/);
  assert.equal(at('2026-12-01T16:05:00Z', { headBuilt: false }).release, false);

  const tenDone = { trainRuns: [run('2026-12-01T16:10:00Z')] };
  assert.equal(at('2026-12-01T16:50:00Z', tenDone).release, false); // same slot
  assert.equal(at('2026-12-01T20:05:00Z', tenDone).release, true); // 14:00 slot

  const both = { trainRuns: [run('2026-12-01T20:10:00Z'), run('2026-12-01T16:10:00Z')] };
  const third = at('2026-12-01T20:40:00Z', both);
  assert.equal(third.release, false);
  assert.match(third.reason, /already 2 releases/);

  // yesterday's runs do not count today
  assert.equal(at('2026-12-01T16:05:00Z', { trainRuns: [run('2026-11-30T16:10:00Z'), run('2026-11-30T20:10:00Z')] }).release, true);
});

test('release train: a maintainer override skips the window but not the daily cap', () => {
  const base = { trainRuns: [], shippableChanged: true, headBuilt: true, ignoreClock: true };
  assert.equal(releaseDecision({ ...base, now: new Date('2026-12-01T18:30:00Z') }).release, true);
  const capped = releaseDecision({ ...base, trainRuns: [run('2026-12-01T16:10:00Z'), run('2026-12-01T20:10:00Z')], now: new Date('2026-12-01T18:30:00Z') });
  assert.equal(capped.release, false);
});

test('release train: a release build that failed in this slot is re-run, at most three attempts, only while the slot is open', () => {
  const run = (over = {}) => ({ id: 1, created_at: '2026-11-24T16:00:16Z', status: 'completed', conclusion: 'failure', run_attempt: 1, ...over });
  const at = (iso) => new Date(iso);
  // 10:40 Central = 16:40 UTC in November
  assert.equal(retryableRun({ now: at('2026-11-24T16:40:00Z'), trainRuns: [run()] })?.id, 1);
  assert.equal(retryableRun({ now: at('2026-11-24T16:40:00Z'), trainRuns: [run({ conclusion: 'cancelled' })] })?.id, 1);
  assert.equal(retryableRun({ now: at('2026-11-24T16:40:00Z'), trainRuns: [run({ run_attempt: 2 })] })?.id, 1);
  assert.equal(retryableRun({ now: at('2026-11-24T16:40:00Z'), trainRuns: [run({ run_attempt: 3 })] }), null);
  assert.equal(retryableRun({ now: at('2026-11-24T16:40:00Z'), trainRuns: [run({ conclusion: 'success' })] }), null);
  assert.equal(retryableRun({ now: at('2026-11-24T16:40:00Z'), trainRuns: [run({ status: 'in_progress', conclusion: null })] }), null);
  // window closed (11:45 Central), or a run from another slot or day
  assert.equal(retryableRun({ now: at('2026-11-24T17:45:00Z'), trainRuns: [run()] }), null);
  assert.equal(retryableRun({ now: at('2026-11-24T20:10:00Z'), trainRuns: [run()] }), null); // 14:10 slot, the failed run is the 10:00 one
  assert.equal(retryableRun({ now: at('2026-11-25T16:40:00Z'), trainRuns: [run()] }), null);
  // only the newest run of the slot counts
  assert.equal(retryableRun({ now: at('2026-11-24T16:40:00Z'), trainRuns: [run({ id: 1 }), run({ id: 2, created_at: '2026-11-24T16:20:00Z', conclusion: 'success' })] }), null);
  assert.equal(retryableRun({ now: at('2026-11-24T16:40:00Z'), trainRuns: [] }), null);
});

test('nextVersion', () => {
  assert.equal(nextVersion('v1.2.0', ['fix: crash', 'docs: x']), 'v1.2.1');
  assert.equal(nextVersion('v1.2.5', ['feat: new thing', 'fix: y']), 'v1.3.0');
  assert.equal(nextVersion('v1.2.5', ['feat(ui): x']), 'v1.3.0');
  assert.equal(nextVersion('v1.2.5', ['fix!: break']), 'v1.3.0');
  assert.throws(() => nextVersion('edge', []));
});
