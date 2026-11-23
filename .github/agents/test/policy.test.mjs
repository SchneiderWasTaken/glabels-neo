import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  addedLinesOf, agentMayChange, chicago, classifyChange, isShippable, maxRisk, mayAutoMerge, nextVersion, parseLevel,
  releaseDecision,
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
    'cmake/Foo.cmake', 'tools/windows-launcher/x.ps1', 'LICENSE', 'THIRD-PARTY-NOTICES.md', 'data/foo.desktop', 'package.json',
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
  assert.deepEqual(r, { risk: 'low', reasons: [] });
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
    'QString name = tr( "Label width" );',
  ];
  for (const line of ok) {
    assert.equal(classifyChange([f('glabels/A.cpp')], [line]).risk, 'low', line);
  }
});

test('addedLinesOf extracts added lines only', () => {
  const diff = ['--- a/x', '+++ b/x', '@@ -1,2 +1,3 @@', ' ctx', '-old', '+new', '++plus'].join('\n');
  assert.deepEqual(addedLinesOf(diff), ['new', '+plus']);
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

const rel = (tag, iso, extra = {}) => ({ tag_name: tag, published_at: iso, draft: false, prerelease: false, ...extra });

test('release train: only at 10:00 and 14:00 Chicago, with changes, once per slot, twice per day', () => {
  const base = { releases: [rel('v1.2.0', '2026-11-20T16:00:00Z')], shippableChanged: true, headBuilt: true };
  const at = (iso) => releaseDecision({ ...base, now: new Date(iso) });

  assert.equal(at('2026-12-01T16:05:00Z').release, true); // 10:05 CST
  assert.equal(at('2026-12-01T20:30:00Z').release, true); // 14:30 CST
  assert.equal(at('2026-12-01T17:00:00Z').release, false); // 11:00
  assert.equal(at('2026-12-01T15:00:00Z').release, false); // 09:00 (the CDT-hour cron during CST)
  assert.equal(at('2026-12-01T21:00:00Z').release, false); // 15:00

  const noChanges = releaseDecision({ ...base, now: new Date('2026-12-01T16:05:00Z'), shippableChanged: false });
  assert.equal(noChanges.release, false);
  assert.match(noChanges.reason, /no user-visible/);

  const unbuilt = releaseDecision({ ...base, now: new Date('2026-12-01T16:05:00Z'), headBuilt: false });
  assert.equal(unbuilt.release, false);

  const tenDone = { ...base, releases: [rel('v1.2.1', '2026-12-01T16:10:00Z'), ...base.releases] };
  assert.equal(releaseDecision({ ...tenDone, now: new Date('2026-12-01T16:50:00Z') }).release, false); // same slot
  assert.equal(releaseDecision({ ...tenDone, now: new Date('2026-12-01T20:05:00Z') }).release, true); // 14:00 slot

  const both = { ...base, releases: [rel('v1.2.2', '2026-12-01T20:10:00Z'), rel('v1.2.1', '2026-12-01T16:10:00Z')] };
  const third = releaseDecision({ ...both, now: new Date('2026-12-01T20:40:00Z') });
  assert.equal(third.release, false);
  assert.match(third.reason, /already 2 releases/);
});

test('release train ignores edge prereleases and drafts when counting', () => {
  const releases = [
    rel('edge', '2026-12-01T16:01:00Z', { prerelease: true }),
    rel('v1.2.3', '2026-12-01T16:02:00Z', { draft: true }),
    rel('v1.2.0', '2026-11-20T16:00:00Z'),
  ];
  assert.equal(releaseDecision({ releases, shippableChanged: true, headBuilt: true, now: new Date('2026-12-01T16:30:00Z') }).release, true);
});

test('nextVersion', () => {
  assert.equal(nextVersion('v1.2.0', ['fix: crash', 'docs: x']), 'v1.2.1');
  assert.equal(nextVersion('v1.2.5', ['feat: new thing', 'fix: y']), 'v1.3.0');
  assert.equal(nextVersion('v1.2.5', ['feat(ui): x']), 'v1.3.0');
  assert.equal(nextVersion('v1.2.5', ['fix!: break']), 'v1.3.0');
  assert.throws(() => nextVersion('edge', []));
});
