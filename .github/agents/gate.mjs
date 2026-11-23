// Decides whether an automation workflow should run at all.
//   node gate.mjs <stage>   stage: triage | fix | merge | release
// Reads AUTOMATION_LEVEL (a repository variable) and, for triage, refuses to run during an issue flood.
// Writes `run=true|false` and `level=N` to $GITHUB_OUTPUT.

import { appendFileSync } from 'node:fs';

import { REPO, api } from './lib.mjs';
import { parseLevel } from './policy.mjs';

const stage = process.argv[2];
const level = parseLevel(process.env.AUTOMATION_LEVEL);
const MIN_LEVEL = { triage: 1, fix: 2, merge: 3, release: 0 };
if (!(stage in MIN_LEVEL)) throw new Error(`unknown stage: ${stage}`);

let run = level >= MIN_LEVEL[stage];
let why = run ? 'enabled' : `automation level ${level} < ${MIN_LEVEL[stage]}`;

// Scheduled releases are opt-in: set the repository variable RELEASE_TRAIN to "on".
if (stage === 'release') {
  // (A maintainer's dry run is always allowed: it only prints the decision.)
  run = process.env.RELEASE_TRAIN === 'on' || process.env.RELEASE_DRY_RUN === 'true';
  why = run ? 'RELEASE_TRAIN is on, or this is a dry run' : 'RELEASE_TRAIN is not "on"';
}

if (run && stage === 'triage') {
  // Flood guard: a burst of issues (spam, or a bug in the reporter) must not burn the model budget.
  const since = new Date(Date.now() - 3600_000).toISOString();
  const recent = api(`repos/${REPO}/issues?state=all&since=${encodeURIComponent(since)}&per_page=100`) ?? [];
  const count = recent.filter((i) => !i.pull_request && new Date(i.created_at) > new Date(since)).length;
  const cap = Number(process.env.TRIAGE_MAX_PER_HOUR || 30);
  if (count > cap) {
    run = false;
    why = `${count} issues in the last hour (cap ${cap})`;
  }
}

if (run && stage === 'fix') {
  // Daily cap on fix attempts (each one builds the project and spends model budget).
  const since = new Date(Date.now() - 24 * 3600_000).toISOString();
  // The `labeled` trigger starts a (skipped) run for every label added to any issue: only runs whose gate let the
  // work through count against the cap, and this run does not count against itself.
  const listed = api(`repos/${REPO}/actions/workflows/agent-fix.yml/runs?created=%3E${encodeURIComponent(since)}&per_page=100`)?.workflow_runs ?? [];
  const runs = listed.filter((r) => r.conclusion !== 'skipped' && String(r.id) !== String(process.env.GITHUB_RUN_ID)).length;
  const cap = Number(process.env.FIX_MAX_PER_DAY || 8);
  if (runs > cap) {
    run = false;
    why = `${runs} fix runs in the last 24 hours (cap ${cap})`;
  }
}

console.log(`gate ${stage}: run=${run} (${why}); level=${level}`);
if (process.env.GITHUB_OUTPUT) appendFileSync(process.env.GITHUB_OUTPUT, `run=${run}\nlevel=${level}\n`);
