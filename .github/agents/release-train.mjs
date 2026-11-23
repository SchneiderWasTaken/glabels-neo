// The release train: at 10:00 and 14:00 Central, publish a stable release if (and only if) something
// user-visible has landed on main since the last stable release.  At most two releases a day.
//
//   RELEASE_DRY_RUN=true   print the decision, change nothing
//   NOW_OVERRIDE=<iso>     pretend it is this time (for rehearsals)
//   RELEASE_FORCE_HOUR=1   ignore the clock (manual run by a maintainer)

import { REPO, api, gh } from './lib.mjs';
import { isShippable, nextVersion, releaseDecision, chicago } from './policy.mjs';

const dry = process.env.RELEASE_DRY_RUN === 'true';
const now = process.env.NOW_OVERRIDE ? new Date(process.env.NOW_OVERRIDE) : new Date();
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const releases = api(`repos/${REPO}/releases?per_page=100`) ?? [];
const stable = releases
  .filter((r) => !r.draft && !r.prerelease && /^v\d+\.\d+\.\d+$/.test(r.tag_name))
  .sort((a, b) => new Date(b.published_at) - new Date(a.published_at));
if (!stable.length) throw new Error('no stable release to compare against');
const lastTag = stable[0].tag_name;

const compare = api(`repos/${REPO}/compare/${lastTag}...main`);
const commits = compare.commits ?? [];
const headSha = api(`repos/${REPO}/commits/main`).sha;
const shippableChanged = commits.length > 0 && (compare.files ?? []).some((f) => isShippable(f.filename));

// Has the newest main commit been built (and its tests run) by the Release workflow?
async function headState() {
  const runs = (api(`repos/${REPO}/actions/workflows/release.yml/runs?head_sha=${headSha}&per_page=20`)?.workflow_runs ?? [])
    .filter((r) => r.head_branch === 'main' && ['push', 'workflow_dispatch'].includes(r.event));
  if (runs.some((r) => r.conclusion === 'success')) return 'built';
  if (runs.some((r) => ['queued', 'in_progress', 'waiting', 'pending'].includes(r.status))) return 'building';
  return 'none';
}

let state = await headState();
const t = chicago(now);
const inSlot = [10, 14].includes(t.hour) || process.env.RELEASE_FORCE_HOUR === '1';
// An edge build that is still running at release time gets up to 40 minutes to finish.
for (let i = 0; i < 40 && state === 'building' && inSlot && shippableChanged && !dry; i++) {
  console.log('waiting for the edge build of main to finish...');
  await sleep(60_000);
  state = await headState();
}

const decision = releaseDecision({ now, releases, shippableChanged, headBuilt: state === 'built' });
if (process.env.RELEASE_FORCE_HOUR === '1' && !decision.release && /not a release hour/.test(decision.reason)) {
  decision.release = shippableChanged && state === 'built';
  decision.reason = 'forced by a maintainer';
}

console.log(JSON.stringify({ now: now.toISOString(), chicago: t, lastTag, commits: commits.length, shippableChanged, head: headSha.slice(0, 8), headState: state, ...decision }));
if (!decision.release) process.exit(0);

const version = nextVersion(lastTag, commits.map((c) => c.commit.message.split('\n')[0]));
console.log(`releasing ${version}${dry ? ' (dry run)' : ''}`);
if (dry) process.exit(0);

api(`repos/${REPO}/git/refs`, { method: 'POST', body: { ref: `refs/tags/${version}`, sha: headSha } });
// A tag created with the workflow token does not trigger push workflows, so start the release explicitly.
gh(['workflow', 'run', 'release.yml', '--repo', REPO, '--ref', version]);
console.log(`dispatched Release for ${version}`);
