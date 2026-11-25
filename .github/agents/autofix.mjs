// The `auto-fix` label: a maintainer's sign-off that an agent may change this issue's code - including a SMALL user-interface
// change, which the pipeline otherwise always leaves to a person - and that the pull request may be merged without one, once
// the council (Opus included), CI and the merge policy agree.
//
// The label is only believed when its LATEST event on the issue is "labeled" by a human account with write access.  Every
// stage that relies on it asks again for itself (fix-step prepare, fix-publish, council-prepare, automerge), so removing
// the label, or a label applied by a bot or by someone without write access, stops the work at the next stage.

import { REPO, api, gh } from './lib.mjs';

export const AUTO_FIX_LABEL = 'auto-fix';
const MAY_WRITE = ['admin', 'write', 'maintain'];

/**
 * Pure decision.
 *   events:        the issue's events (GitHub REST), oldest first
 *   labelsNow:     names of the labels the issue has right now
 *   permissionOf:  login -> 'admin' | 'write' | ... (looked up only for the person who applied the label)
 *   lastEditedAt:  when the issue text was last edited (ISO), null if never, undefined if it could not be read (= no)
 * Approval is for the issue AS IT WAS when the label was applied: any later edit of the text voids it.
 * Returns { approved, by, why }.
 */
export function decideApproval(events, labelsNow, permissionOf, lastEditedAt) {
  if (!labelsNow.includes(AUTO_FIX_LABEL)) return { approved: false, by: null, why: 'the issue does not have the auto-fix label' };
  const mine = (events ?? []).filter((e) => ['labeled', 'unlabeled'].includes(e.event) && e.label?.name === AUTO_FIX_LABEL);
  const last = mine[mine.length - 1];
  if (!last || last.event !== 'labeled') return { approved: false, by: null, why: 'no record of who applied the auto-fix label' };
  const actor = last.actor;
  if (!actor?.login || actor.type !== 'User' || last.performed_via_github_app) return { approved: false, by: actor?.login ?? null, why: 'the auto-fix label was not applied by a person' };
  const at = Date.parse(last.created_at);
  if (!Number.isFinite(at)) return { approved: false, by: actor.login, why: 'cannot tell when the auto-fix label was applied' };
  if (lastEditedAt === undefined) return { approved: false, by: actor.login, why: 'could not read when the issue text was last edited' };
  if (lastEditedAt !== null && Date.parse(lastEditedAt) > at) return { approved: false, by: actor.login, why: 'the issue text was edited after the auto-fix label was applied' };
  if ((events ?? []).some((e) => e.event === 'renamed' && Date.parse(e.created_at) > at)) return { approved: false, by: actor.login, why: 'the issue was renamed after the auto-fix label was applied' };
  if (!MAY_WRITE.includes(permissionOf(actor.login))) return { approved: false, by: actor.login, why: `${actor.login} cannot approve auto-fix (no write access)` };
  return { approved: true, by: actor.login, why: 'ok' };
}

/** Ask GitHub: is `auto-fix` approved on this issue right now? */
export function autoFixApproval(issueNumber) {
  const issue = api(`repos/${REPO}/issues/${issueNumber}`);
  if (!issue || issue.pull_request) return { approved: false, by: null, why: 'not an issue' };
  const events = api(`repos/${REPO}/issues/${issueNumber}/events?per_page=100`, { paginate: true, allowFail: true }) ?? [];
  const labelsNow = (issue.labels ?? []).map((l) => l.name);
  return decideApproval(events, labelsNow, (login) =>
    api(`repos/${REPO}/collaborators/${encodeURIComponent(login)}/permission`, { allowFail: true })?.permission ?? 'none',
  lastEdited(issueNumber));
}

/** When the issue text (body) was last edited: an ISO time, null if never, undefined if GitHub could not be asked. */
function lastEdited(issueNumber) {
  const [owner, name] = REPO.split('/');
  const out = gh(['api', 'graphql', '-f', 'query=query($o:String!,$n:String!,$i:Int!){repository(owner:$o,name:$n){issue(number:$i){lastEditedAt}}}',
    '-f', `o=${owner}`, '-f', `n=${name}`, '-F', `i=${issueNumber}`], { allowFail: true });
  try {
    const v = JSON.parse(out ?? '').data.repository.issue.lastEditedAt;
    return v ?? null;
  } catch {
    return undefined;
  }
}
