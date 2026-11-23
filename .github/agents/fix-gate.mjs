// Eligibility for a fix attempt.  Writes run=true|false, issue=N and body_sha=<fingerprint> to $GITHUB_OUTPUT.
//
// Env: EVENT (workflow_dispatch | issues), ACTOR, ISSUE, EXPECTED_BODY_SHA (from triage, optional), LEVEL_OK.
// The fingerprint pins the exact issue text this attempt is allowed to use; later stages refuse to continue if the
// issue was edited since.

import { appendFileSync } from 'node:fs';

import { REPO, api, issueHash } from './lib.mjs';

const issueNumber = Number(process.env.ISSUE);
const out = (run, why, bodySha = '') => {
  console.log(`eligible: ${run} (${why})`);
  if (process.env.GITHUB_OUTPUT) appendFileSync(process.env.GITHUB_OUTPUT, `run=${run}\nissue=${issueNumber}\nbody_sha=${bodySha}\n`);
  process.exit(0);
};

if (!Number.isInteger(issueNumber) || issueNumber <= 0) out(false, 'no issue number');
if (process.env.LEVEL_OK !== 'true') out(false, 'automation level too low, or the daily cap was reached');

// Who asked?  The automation itself (through triage) or someone with write access.
const actor = process.env.ACTOR ?? '';
if (actor !== 'github-actions[bot]') {
  const perm = api(`repos/${REPO}/collaborators/${encodeURIComponent(actor)}/permission`, { allowFail: true })?.permission ?? 'none';
  if (!['admin', 'write', 'maintain'].includes(perm)) out(false, `actor ${actor} has permission '${perm}'`);
}

const issue = api(`repos/${REPO}/issues/${issueNumber}`);
if (issue.pull_request) out(false, 'that is a pull request');
if (issue.state !== 'open') out(false, `issue is ${issue.state}`);

const open = api(`repos/${REPO}/pulls?state=open&per_page=100`) ?? [];
if (open.some((p) => p.head.ref.startsWith(`agent/issue-${issueNumber}-`))) out(false, 'an agent PR is already open for this issue');

const sha = issueHash(issue.title, issue.body);
const expected = process.env.EXPECTED_BODY_SHA ?? '';
if (expected && expected !== sha) out(false, 'the issue was edited after triage');

out(true, 'ok', sha);
