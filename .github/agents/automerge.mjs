// Merge policy: merge ONE council-approved agent PR if every condition holds right now.
//
// Conditions (all must be true, evaluated against the exact head commit):
//   - PR was opened by the automation on an agent/issue-* branch, targets main, is not a draft
//   - status "Council" is success on the head commit; checks "CI OK" and "Lint OK" succeeded on it
//   - the change, re-classified from its diff right now, is not "blocked"
//   - policy.mayAutoMerge(risk, AUTOMATION_LEVEL, now) allows it (high risk only in the morning window)
// Runs without model credentials.  Merges are rebase merges pinned to the checked commit.

import { REPO, api, gh } from './lib.mjs';
import { parseDiff } from './diff-lib.mjs';
import { classifyChange, mayAutoMerge, maxRisk, parseLevel } from './policy.mjs';

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

  const sha = p.head.sha;

  // The reviewed and tested tree must be what will land: the branch has to be based on the current main.
  const cmp = api(`repos/${REPO}/compare/main...${sha}`);
  if (cmp.behind_by > 0) { skip(`main moved on (${cmp.behind_by} commits behind); the change must be redone or rebased by a human`); continue; }
  const status = api(`repos/${REPO}/commits/${sha}/status`);
  const council = (status.statuses ?? []).filter((s) => s.context === 'Council').sort((a, b) => new Date(b.updated_at) - new Date(a.updated_at))[0];
  if (council?.state !== 'success') { skip(`Council status is ${council?.state ?? 'missing'} for this commit`); continue; }

  const runs = api(`repos/${REPO}/commits/${sha}/check-runs?per_page=100`).check_runs ?? [];
  const bad = REQUIRED_CHECKS.map((n) => [n, latestConclusion(runs, n)]).filter(([, c]) => c !== 'success');
  if (bad.length) { skip(`checks not green: ${bad.map(([n, c]) => `${n}=${c}`).join(', ')}`); continue; }

  const diffText = gh(['pr', 'diff', String(p.number), '--repo', REPO]);
  if (api(`repos/${REPO}/pulls/${p.number}`).head.sha !== sha) { skip('the PR changed while it was being checked'); continue; }
  const { files, addedLines } = parseDiff(diffText);
  const fresh = classifyChange(files, addedLines, { diffChars: diffText.length });
  const labelled = ['blocked', 'high', 'low'].find((r) => labels.includes(`risk:${r}`)) ?? 'blocked';
  // The label can only make the class stricter than the fresh analysis, never looser.
  const risk = maxRisk(fresh.risk, labelled);

  const decision = mayAutoMerge(risk, level, now);
  if (!decision.ok) { skip(`${decision.reason} (risk ${risk})`); continue; }

  console.log(`${tag}: merging (${decision.reason}, risk ${risk})`);
  try {
    gh(['pr', 'merge', String(p.number), '--repo', REPO, '--rebase', '--match-head-commit', sha, '--delete-branch']);
  } catch (e) {
    // e.g. a rebase conflict: do not let one stuck PR starve the others; ask for a human and move on.
    console.log(`${tag}: merge failed (${String(e.message).slice(0, 200)})`);
    api(`repos/${REPO}/issues/${p.number}/labels`, { method: 'POST', body: { labels: ['needs-human'] } });
    continue;
  }

  // A merge made with the workflow token does not trigger the push-based edge build; start it.
  gh(['workflow', 'run', 'release.yml', '--repo', REPO, '--ref', 'main', '-f', 'publish=edge']);

  const issue = Number(/Fixes #(\d+)/.exec(p.body ?? '')?.[1]);
  if (Number.isInteger(issue)) {
    api(`repos/${REPO}/issues/${issue}/comments`, {
      method: 'POST',
      body: { body: `Fixed in ${tag}. It is in the newest edge build now and will ship in the next scheduled release (10:00 or 14:00 Central).` },
    });
  }
  break; // one merge per run: the next PR is checked against the new main on the next run
}
