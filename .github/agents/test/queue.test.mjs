import assert from 'node:assert/strict';
import { test } from 'node:test';

import { isOnlyWaitingFor } from '../council-lib.mjs';
import { MARKER_RE, MAX_PARKS, holdComment, makeMarker, parseMarker, pickNext, priorityOf } from '../queue-lib.mjs';

const NOW = Date.parse('2026-11-25T12:00:00Z');
const SHA = 'a'.repeat(64);

test('a marker survives a round trip and only accepts what it should', () => {
  const m = makeMarker({ kind: 'fix', until: NOW, sha: SHA, parks: 3 });
  assert.deepEqual(parseMarker(`hello\n${m}\nbye`), { kind: 'fix', until: NOW, sha: SHA, parks: 3 });
  assert.deepEqual(parseMarker(makeMarker({ kind: 'triage', until: NOW })), { kind: 'triage', until: NOW, sha: '', parks: 1 });
  assert.equal(parseMarker('<!-- agent-queue kind=rm until=2026-11-25T12:00:00Z -->'), null);
  assert.equal(parseMarker('<!-- agent-queue kind=fix until=tomorrow -->'), null);
  assert.equal(parseMarker('no marker'), null);
  assert.equal(parseMarker(undefined), null);
  assert.throws(() => makeMarker({ kind: 'nope', until: NOW }), /unknown queue kind/);
  assert.throws(() => makeMarker({ kind: 'fix', until: NOW, sha: 'xyz' }), /sha/);
  assert.ok(MARKER_RE.test(holdComment({ kind: 'council', until: NOW })));
  assert.match(holdComment({ kind: 'fix', until: NOW }), /not counted as a failure/);
});

test('the most important work goes first', () => {
  const order = [
    { kind: 'fix', labels: ['blocks-user', 'agent-ready'] }, { kind: 'council', labels: [] }, { kind: 'triage', labels: [] },
    { kind: 'fix', labels: ['agent-fix'] }, { kind: 'fix', labels: ['agent-ready'] }, { kind: 'fix', labels: ['auto-fix'] }, { kind: 'fix', labels: [] },
  ];
  const ranks = order.map(priorityOf);
  assert.deepEqual(ranks, [...ranks].sort((a, b) => a - b));
  assert.equal(new Set(ranks).size, ranks.length);
  assert.equal(priorityOf({ kind: 'fix', labels: ['blocks-user', 'agent-fix'] }), 0);
});

test('nothing starts while any limit is still running, then the best item goes, oldest first', () => {
  const item = (number, kind, labels, until, createdAt = '2026-11-20T00:00:00Z') => ({ number, kind, labels, until, createdAt, sha: '' });
  const waiting = pickNext([item(1, 'fix', ['blocks-user'], NOW + 60_000), item(2, 'fix', [], NOW - 1)], NOW);
  assert.equal(waiting.next, null);
  assert.equal(waiting.holdUntil, NOW + 60_000);

  const ready = [
    item(5, 'fix', ['auto-fix'], NOW - 1), item(3, 'fix', ['agent-ready'], NOW - 1, '2026-11-01T00:00:00Z'),
    item(9, 'fix', ['blocks-user'], NOW - 1, '2026-11-24T00:00:00Z'), item(8, 'fix', ['blocks-user'], NOW - 1, '2026-11-22T00:00:00Z'),
  ];
  assert.equal(pickNext(ready, NOW).next.number, 8); // blocks-user beats everything; the older one first
  assert.equal(pickNext([], NOW).next, null);
  assert.equal(pickNext([item(4, 'triage', [], NOW - 1), item(2, 'fix', ['agent-ready'], NOW - 1)], NOW).next.number, 4);
});

test('a stuck item is eventually handed to a person', () => {
  assert.ok(MAX_PARKS >= 6 && MAX_PARKS <= 24);
});

test('a review is only postponed when nobody who answered objects', () => {
  const ok = { verdict: 'approve', risk: 'low', scope_ok: true, backdoor_suspected: false, concerns: [] };
  assert.equal(isOnlyWaitingFor([{ id: 'a', verdict: ok }], 'low'), true);
  assert.equal(isOnlyWaitingFor([], 'low'), true);
  assert.equal(isOnlyWaitingFor([{ id: 'a', verdict: ok }], 'blocked'), false);
  for (const bad of [{ verdict: 'request_changes' }, { backdoor_suspected: true }, { scope_ok: false }, { risk: 'blocked' }, { concerns: [{ severity: 'blocking', text: 'x' }] }]) {
    assert.equal(isOnlyWaitingFor([{ id: 'a', verdict: ok }, { id: 'b', verdict: { ...ok, ...bad } }], 'low'), false, JSON.stringify(bad));
  }
  assert.equal(isOnlyWaitingFor([{ id: 'a', verdict: { ...ok, concerns: [{ severity: 'minor', text: 'nit' }] } }], 'low'), true);
});
