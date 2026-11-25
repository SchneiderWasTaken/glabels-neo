// Merge policy: merge ONE council-approved agent PR if every condition holds right now.
//
// Conditions (all must be true, evaluated against the exact head commit):
//   - PR was opened by the automation on an agent/issue-* branch, targets main, is not a draft
//   - status "Council" is success on the head commit; checks "CI OK" and "Lint OK" succeeded on it
//   - the issue it fixes is labelled `blocks-user` (a crash, failed print ... that stops someone working) and is not
//     a UI change, a feature request or marked for human review: those are decisions for a person
//   - the change, re-classified from its diff right now, is not "blocked" (which includes any UI change)
//   - policy.mayAutoMerge(risk, AUTOMATION_LEVEL, now) allows it (high risk only in the morning window)
// Runs without model credentials.  Merges are rebase merges pinned to the checked commit.

import { autoFixApproval } from './autofix.mjs';
import { REPO, api, gh } from './lib.mjs';
import { parseDiff } from './diff-lib.mjs';
import { classifyChange, isShippable, mayAutoMerge, maxRisk, parseLevel } from './policy.mjs';

const level = parseLevel(process.env.AUTOMATION_LEVEL);
const now = new Date();
const REQUIRED_CHECKS = ['CI OK', 'Lint OK'];

const prs = api(`repos/${REPO}/pulls?state=open&base=main&per_page=50`) ?? [];
console.log(`${prs.length} open PRs, automation level ${level}`);

function latestConclusion(runs, name) {
  const mine = runs.filter((r) => r.name === name).sort((a, b) => new Date(b.started_at) - new Date(a.started_at));
  return mine[0]?.conclusion ?? (mine[0] ? 'pending' : 'missing');
}

for (const p of prs) {
  const labels = p.labels.map((l) => l.name);
  const tag = `#${p.number}`;
  if (!labels.includes('agent-pr')) continue;
  const skip = (why) => console.log(`${tag}: not merging - ${why}`);

  if (p.draft) { skip('still a draft'); continue; }
  if (!labels.includes('council-approved')) { skip('no council approval'); continue; }
  if (p.user.login !== 'github-actions[bot]' || !p.head.ref.startsWith('agent/issue-') || p.head.repo?.full_name !== REPO) { skip('not an automation branch'); continue; }

  // Only fixes for problems that block a user are merged without a person.  Everything else waits for a human.
  const fixes = Number(/Fixes #(\d+)/.exec(p.body ?? '')?.[1]);
  if (!Number.isInteger(fixes)) { skip('the PR does not name the issue it fixes'); continue; }
  const issueLabels = (api(`repos/${REPO}/issues/${fixes}`)?.labels ?? []).map((l) => l.name);
  // A maintainer's `auto-fix` label (applied by a person with write access, asked again right now) is the decision itself:
  // then neither blocks-user nor the UI holds apply, but the interface rules, the Opus review and every check still do.
  const approval = autoFixApproval(fixes);
  const autoFix = approval.approved;
  if (autoFix && !p.head.ref.startsWith(`agent/issue-${fixes}-`)) { skip(`the branch is not for issue #${fixes}`); continue; }
  if (autoFix) {
    if (issueLabels.includes('needs-human')) { skip(`issue #${fixes} is labelled needs-human`); continue; }
  } else {
    if (!issueLabels.includes('blocks-user')) { skip(`issue #${fixes} is not labelled blocks-user: a person decides whether to merge it`); continue; }
    const heldFor = ['ui-change', 'enhancement', 'needs-human-review', 'needs-human'].filter((l) => issueLabels.includes(l));
    if (heldFor.length) { skip(`issue #${fixes} is labelled ${heldFor.join(', ')}: a person decides`); continue; }
  }

  const sha = p.head.sha;

  // The reviewed and tested tree must be what will land: the branch has to be based on the current main.
  const cmp = api(`repos/${REPO}/compare/main...${sha}`);
  if (cmp.behind_by > 0) { skip(`main moved on (${cmp.behind_by} commits behind); the change must be redone or rebased by a human`); continue; }
  const status = api(`repos/${REPO}/commits/${sha}/status`);
  const council = (status.statuses ?? []).filter((s) => s.context === 'Council').sort((a, b) => new Date(b.updated_at) - new Date(a.updated_at))[0];
  if (council?.state !== 'success') { skip(`Council status is ${council?.state ?? 'missing'} for this commit`); continue; }
  if (autoFix) {
    const opus = (status.statuses ?? []).filter((s) => s.context === 'Council (auto-fix)').sort((a, b) => new Date(b.updated_at) - new Date(a.updated_at))[0];
    if (opus?.state !== 'success') { skip(`Council (auto-fix) status is ${opus?.state ?? 'missing'}: the required reviewers have not all approved`); continue; }
  }

  const runs = api(`repos/${REPO}/commits/${sha}/check-runs?per_page=100`).check_runs ?? [];
  const bad = REQUIRED_CHECKS.map((n) => [n, latestConclusion(runs, n)]).filter(([, c]) => c !== 'success');
  if (bad.length) {
    // Housekeeping so a PR is never stuck on infrastructure: approve held runs, and re-run a failed CI/Lint once
    // (runner flakes happen; a genuine failure fails again and stays failed).
    const wruns = api(`repos/${REPO}/actions/runs?head_sha=${sha}&per_page=50`)?.workflow_runs ?? [];
    for (const r of wruns.filter((x) => x.conclusion === 'action_required')) {
      api(`repos/${REPO}/actions/runs/${r.id}/approve`, { method: 'POST', allowFail: true });
      console.log(`${tag}: approved held run ${r.id} (${r.name})`);
    }
    for (const r of wruns.filter((x) => ['CI', 'Lint'].includes(x.name) && x.conclusion === 'failure' && x.run_attempt < 2)) {
      gh(['run', 'rerun', String(r.id), '--repo', REPO, '--failed'], { allowFail: true });
      console.log(`${tag}: re-running failed ${r.name} run ${r.id} once`);
    }
    skip(`checks not green: ${bad.map(([n, c]) => `${n}=${c}`).join(', ')}`);
    continue;
  }

  const diffText = gh(['pr', 'diff', String(p.number), '--repo', REPO]);
  if (api(`repos/${REPO}/pulls/${p.number}`).head.sha !== sha) { skip('the PR changed while it was being checked'); continue; }
  const { files, addedLines } = parseDiff(diffText);
  const fresh = classifyChange(files, addedLines, { diffChars: diffText.length, autoFix });
  const labelled = ['blocked', 'high', 'low'].find((r) => labels.includes(`risk:${r}`)) ?? 'blocked';
  // The label can only make the class stricter than the fresh analysis, never looser.
  const risk = maxRisk(fresh.risk, labelled);

  const decision = mayAutoMerge(risk, level, now);
  if (!decision.ok) { skip(`${decision.reason} (risk ${risk})`); continue; }

  // Asked once more right before the merge: a label removed meanwhile must stop it.
  if (autoFix && !autoFixApproval(fixes).approved) { skip('the auto-fix approval was withdrawn, or the issue is now marked needs-human'); continue; }

  console.log(`${tag}: merging (${decision.reason}, risk ${risk}${autoFix ? `, auto-fix approved by ${approval.by}` : ''})`);
  try {
    gh(['pr', 'merge', String(p.number), '--repo', REPO, '--rebase', '--match-head-commit', sha, '--delete-branch']);
  } catch (e) {
    // e.g. a rebase conflict: do not let one stuck PR starve the others; ask for a human and move on.
    console.log(`${tag}: merge failed (${String(e.message).slice(0, 200)})`);
    api(`repos/${REPO}/issues/${p.number}/labels`, { method: 'POST', body: { labels: ['needs-human'] } });
    continue;
  }

  // A merge made with the workflow token does not trigger the push-based edge build; start it.
  if (files.some((f) => isShippable(f.path))) {
    gh(['workflow', 'run', 'release.yml', '--repo', REPO, '--ref', 'main', '-f', 'publish=edge']);
  } else {
    console.log(`${tag}: nothing user-visible changed; no edge build needed`);
  }

  const issue = Number(/Fixes #(\d+)/.exec(p.body ?? '')?.[1]);
  if (Number.isInteger(issue)) {
    api(`repos/${REPO}/issues/${issue}/comments`, {
      method: 'POST',
      body: { body: `Fixed in ${tag}. It is in the newest edge build now and will ship in the next scheduled release (10:00 or 14:00 Central).` },
    });
  }
  break; // one merge per run: the next PR is checked against the new main on the next run
}
