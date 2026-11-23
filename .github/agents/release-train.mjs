// The release train: at 10:00 and 14:00 Central (inside a window, because GitHub's scheduler can start late), publish a
// stable release if - and only if - something user-visible has landed on main since the last stable release.
// At most two releases a day, one per slot.
//
//   RELEASE_DRY_RUN=true   print the decision, change nothing
//   NOW_OVERRIDE=<iso>     pretend it is this time (for rehearsals)
//   RELEASE_FORCE_HOUR=1   ignore the clock window (maintainer override; the daily cap still applies)

import { REPO, api, gh } from './lib.mjs';
import { chicago, isShippable, nextVersion, releaseDecision } from './policy.mjs';

const dry = process.env.RELEASE_DRY_RUN === 'true';
const now = process.env.NOW_OVERRIDE ? new Date(process.env.NOW_OVERRIDE) : new Date();
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// One consistent view of main: everything below refers to this exact commit.
const headSha = api(`repos/${REPO}/commits/main`).sha;

const releases = api(`repos/${REPO}/releases?per_page=100`) ?? [];
const stable = releases
  .filter((r) => !r.draft && !r.prerelease && /^v\d+\.\d+\.\d+$/.test(r.tag_name))
  .sort((a, b) => new Date(b.published_at) - new Date(a.published_at));
if (!stable.length) throw new Error('no stable release to compare against');
const lastTag = stable[0].tag_name;

const compare = api(`repos/${REPO}/compare/${lastTag}...${headSha}`);
const commits = compare.commits ?? [];
const truncated = (compare.total_commits ?? commits.length) > commits.length || (compare.files ?? []).length >= 300;
const shippableChanged = commits.length > 0 && ((compare.files ?? []).some((f) => isShippable(f.filename)) || truncated);

// Slots used = release runs on version tags (any outcome) created today, Chicago time.
const releaseRuns = api(`repos/${REPO}/actions/workflows/release.yml/runs?per_page=100`)?.workflow_runs ?? [];
const trainRuns = releaseRuns.filter((r) => /^v\d+\.\d+\.\d+$/.test(r.head_branch ?? '') && r.event === 'workflow_dispatch');

// Is the code on main built and tested?  The Release workflow does not run for documentation-only changes, so look
// for the newest successful build on main and accept it if nothing shippable changed since.
async function buildState() {
  const runs = releaseRuns.filter((r) => r.head_branch === 'main' && ['push', 'workflow_dispatch'].includes(r.event));
  const ok = runs.filter((r) => r.conclusion === 'success').sort((a, b) => new Date(b.created_at) - new Date(a.created_at))[0];
  if (ok) {
    if (ok.head_sha === headSha) return 'built';
    const since = api(`repos/${REPO}/compare/${ok.head_sha}...${headSha}`);
    const files = since.files ?? [];
    if ((since.total_commits ?? 0) <= (since.commits ?? []).length && files.length < 300 && !files.some((f) => isShippable(f.filename))) {
      return 'built';
    }
  }
  const running = runs.some((r) => ['queued', 'in_progress', 'waiting', 'pending'].includes(r.status) && r.head_sha === headSha);
  return running ? 'building' : 'none';
}

let state = await buildState();
// An edge build that is still running at release time gets up to 40 minutes to finish.
for (let i = 0; i < 40 && state === 'building' && shippableChanged && !dry; i++) {
  console.log('waiting for the edge build of main to finish...');
  await sleep(60_000);
  const fresh = api(`repos/${REPO}/actions/workflows/release.yml/runs?head_sha=${headSha}&per_page=20`)?.workflow_runs ?? [];
  releaseRuns.length = 0;
  releaseRuns.push(...fresh);
  state = await buildState();
}

const decision = releaseDecision({
  now, trainRuns, shippableChanged, headBuilt: state === 'built', ignoreClock: process.env.RELEASE_FORCE_HOUR === '1',
});

console.log(JSON.stringify({
  now: now.toISOString(), chicago: chicago(now), lastTag, commits: commits.length, shippableChanged,
  head: headSha.slice(0, 8), buildState: state, slotsUsedToday: trainRuns.filter((r) => chicago(new Date(r.created_at)).date === chicago(now).date).length, ...decision,
}));
if (!decision.release) process.exit(0);

// First free version number: a tag can exist without a release when an earlier build failed.
let version = nextVersion(lastTag, commits.map((c) => c.commit.message.split('\n')[0]));
for (let guard = 0; guard < 20; guard++) {
  const exists = api(`repos/${REPO}/git/ref/tags/${version}`, { allowFail: true });
  if (!exists) break;
  const [maj, min, pat] = version.slice(1).split('.').map(Number);
  version = `v${maj}.${min}.${pat + 1}`;
}
console.log(`releasing ${version}${dry ? ' (dry run)' : ''}`);
if (dry) process.exit(0);

api(`repos/${REPO}/git/refs`, { method: 'POST', body: { ref: `refs/tags/${version}`, sha: headSha } });
// A tag created with the workflow token does not trigger push workflows, so start the release explicitly.
gh(['workflow', 'run', 'release.yml', '--repo', REPO, '--ref', version]);
console.log(`dispatched Release for ${version}`);
