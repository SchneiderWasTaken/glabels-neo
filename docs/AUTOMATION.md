# Automation: from a user's report to a shipped fix

gLabels Neo can take a problem from a user's click on **Report a Problem** all the way to a released fix without a
human in the loop - and a maintainer can turn the amount of autonomy up or down with one setting.

```
 user ──► Report a Problem (app) ──► relay ──► GitHub issue [from-app, needs-triage]
                                                   │
                                          ┌────────▼─────────┐  level ≥ 1
                                          │ 1. Triage        │  labels + comment (model has no tools)
                                          └────────┬─────────┘
                       maintainer adds `agent-fix` │ or (level ≥ 3) triage marked it `agent-ready`
                                          ┌────────▼─────────┐  level ≥ 2
                                          │ 2. Fix agent     │  edits files, builds, tests → DRAFT PR
                                          └────────┬─────────┘
                                          ┌────────▼─────────┐
                                          │ 3. Council       │  several models review the diff
                                          └────────┬─────────┘
                                          ┌────────▼─────────┐  level ≥ 3 (low risk) / 4 (high risk, morning only)
                                          │ 4. Merge policy  │  CI OK + Lint OK + Council on the exact commit
                                          └────────┬─────────┘
                                          ┌────────▼─────────┐  edge build on every merge
                                          │ 5. Release train │  10:00 and 14:00 Central, max 2 a day, only if changed
                                          └──────────────────┘
```

See [REPORTING.md](REPORTING.md) for what a user sends, and [`relay/README.md`](../relay/README.md) for the relay.

## The dial: `AUTOMATION_LEVEL`

Set the repository variable **`AUTOMATION_LEVEL`** (Settings > Secrets and variables > Actions > Variables).
Unset means **1**.

| Level | What agents may do on their own |
|:--:|:--|
| **0** | Nothing. |
| **1** | Triage new issues: labels and a first comment. No code is touched. |
| **2** | + Prepare **draft** fixes when a maintainer adds the `agent-fix` label. Humans review and merge. |
| **3** | + Pick up issues that triage marks `agent-ready`. **Low-risk** PRs that the council approves and CI passes are merged automatically. |
| **4** | + **High-risk** PRs that the council approves are merged automatically, but only 05:00-09:00 Central, so they ship in the 10:00 release and have a day to be watched. |

Changes classified **blocked** are never merged automatically at any level. Turning the level down takes effect on the
next run; nothing needs redeploying.

Other switches (repository variables):

| Variable | Effect |
|:--|:--|
| `RELEASE_TRAIN` | `on` enables scheduled stable releases (default: off). |
| `REPORT_ENDPOINT` | the relay URL embedded in builds. Empty = the app only offers the browser flow. |
| `RELAY_DEPLOY` | `true` lets the relay deploy workflow run. |
| `FIX_MODEL`, `TRIAGE_MODEL` | model alias for the fix agent / triage (default `sonnet`). |
| `COUNCIL_OPENROUTER_MODELS`, `COUNCIL_GEMINI_MODEL` | extra council members (see below). |
| `TRIAGE_MAX_PER_HOUR`, `FIX_MAX_PER_DAY` | budget guards (defaults 30 and 8). |

To stop everything instantly: set `AUTOMATION_LEVEL` to `0`, and `ENABLED = "false"` in the relay.

## 1. Triage (`triage.yml`)

On every new issue a model reads the text - with **no tools at all** - and answers with a JSON object: category, area,
severity, whether it is reproducible, whether an agent could plausibly fix it, questions to ask, possible duplicates.
A second job, which has no model credential, validates that answer and applies the labels and a *template-driven*
comment. Model text appears only in short, sanitised fields. It never closes issues.

## 2. Fix agent (`agent-fix.yml`)

Starts when a maintainer adds `agent-fix` (checked: the person must have write access), or when triage marks the issue
`agent-ready` at level 3+. In one job:

1. builds the baseline;
2. lets a model **read and edit files only** (no shell, no network tool; reads outside the checkout are denied);
3. builds and runs the unit tests itself - with all credentials removed from the environment - and feeds failures back
   (up to 3 attempts);
4. classifies the diff (below) and scans it for secrets.

A separate job, with no model credential, re-validates the patch and opens a **draft PR** from an `agent/issue-N-*`
branch. Daily cap: `FIX_MAX_PER_DAY`.

### What an agent may change - the risk classes (`.github/agents/policy.mjs`)

| Class | Meaning | Examples |
|:--|:--|:--|
| **low** | small, local, ordinary | a UI fix, a label template, a translation, a one-file bug fix with a test |
| **high** | sensitive area or large | printing, file format, mail-merge, barcodes, kiosk, >120 changed lines or >5 files, deleting a file |
| **blocked** | a human must decide | any path outside the allowed source directories (build files, scripts, `.github`, packaging, licenses, dotfiles), binary files, executable bits, removed tests, >600 lines or >12 files, or **added lines that mention** process execution, networking, dynamic loading, environment variables, URLs or long encoded strings |

## 3. Council (`council.yml`)

Independent reviewers see the same diff, the issue (marked untrusted) and the deterministic classification, and answer
with a structured verdict. **Approval requires every participating reviewer to approve** (any `request_changes`,
backdoor suspicion, scope complaint or blocking concern is a veto), at least two to take part, and the combined risk not
to be `blocked`. The result is a commit status **`Council`** on the exact reviewed commit - a later push invalidates it.

Members: Claude `opus` and `haiku` (using the Claude subscription token), plus - when their secret exists - OpenRouter
models (default `~deepseek/deepseek-flash-latest` and `~z-ai/glm-flash-latest`, from `OPENROUTER_API_KEY`) and Gemini
(`GEMINI_API_KEY` + variable `COUNCIL_GEMINI_MODEL`). To add or change members, edit
`COUNCIL_OPENROUTER_MODELS` (comma-separated) - no code change needed.

## 4. Merge policy (`automerge.yml`)

Every 20 minutes and whenever the council approves, **one** PR is merged if all of this holds on its head commit:
opened by the automation on an `agent/issue-*` branch; not a draft; `Council` = success; `CI OK` and `Lint OK` = success;
re-classified risk is not `blocked`; and the level/time rule allows it (low: level 3+ any time; high: level 4, 05:00-09:00
Central). It is a rebase merge pinned to the checked commit, and starts the edge build (merges made with the workflow
token do not trigger push workflows).

## 5. Release train (`release-train.yml`)

Stable releases are cut at **10:00 and 14:00 Central** (the cron runs at both UTC offsets; the script checks the real
Chicago clock), **at most two a day**, and **only if** something user-visible landed on main since the last release and
the newest commit has a successful build. No changes, no release.

- **10:00** carries everything merged since the last release, including the riskier changes (merged 05:00-09:00).
- **14:00** carries the day's low-risk follow-ups, so the build that stands overnight is the most stable one.

Version: minor bump if any commit since the last release is `feat:`/breaking, otherwise patch. The tag triggers the
existing release pipeline (build, test, package, smoke-test, checksums, provenance attestation).
Enable with `RELEASE_TRAIN=on`; rehearse with *Actions > Release train > Run workflow* (dry run).
Edge builds (the newest `main`) are separate and are published on every merge; they are not counted as releases.

## Trust model, in one paragraph

Everything a user writes - report, issue, comment - is untrusted text. Models that see it either have **no tools** (triage,
council) or can only **read and edit files** (fix agent) and run in jobs where the only thing worth stealing is the model
credential, with no network tool to send it anywhere; their outputs are validated against schemas, sanitised and
scanned for credentials before anything is posted or pushed. Jobs that can write to the repository never hold the model
credential. Code that an agent writes is built and tested with credentials stripped from the environment, is reviewed by
several models, must pass CI, and may only touch application source. The set of things automation can never do alone is
in `policy.mjs` and covered by tests.

## Operating notes

- Secrets used: `CLAUDE_OAUTH_TOKEN` (required), `OPENROUTER_API_KEY`, `GEMINI_API_KEY` (optional council members), the
  relay's `RELAY_*` and Cloudflare secrets.
- Repository setting needed: *Settings > Actions > General > Workflow permissions > Allow GitHub Actions to create and
  approve pull requests*.
- Branch protection: `main` requires `CI OK` and `Lint OK`; repository admins can bypass for maintenance.
- Labels used: `from-app`, `needs-triage`, `triaged`, `needs-info`, `needs-human`, `agent-ready`, `agent-fix`, `agent-pr`,
  `council-approved`, `council-rejected`, `risk:low|high|blocked`, `area:*`.
