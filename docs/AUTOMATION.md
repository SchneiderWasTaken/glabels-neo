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
| `MODEL_POOL` | optional comma-separated model ids (`deepseek`, `glm`, `claude-sonnet`, `claude-haiku`) to restrict triage and fixes to. It can only narrow the pool (see *Models and cost*). |
| `TRIAGE_MAX_PER_HOUR`, `FIX_MAX_PER_DAY` | budget guards (defaults 30 and 8). |

To stop everything instantly: set `AUTOMATION_LEVEL` to `0`, and `ENABLED = "false"` in the relay.

## 1. Triage (`triage.yml`)

On every new issue a model reads the text - with **no tools at all** - and answers with a JSON object: category, area,
severity, whether it is reproducible, whether an agent could plausibly fix it, questions to ask, possible duplicates.
A second job, which has no model credential, validates that answer and applies the labels and a *template-driven*
comment. Model text appears only in short, sanitised fields. It never closes issues.

## 2. Fix agent (`agent-fix.yml`)

Starts when a maintainer adds `agent-fix` (checked: the person must have write access), or when triage marks the issue
`agent-ready` at level 3+. Triage marks only issues that **block a user** (a crash, a failed or wrong print, a file that will
not open, a missing label template) as `agent-ready` and labels them `blocks-user`, severe ones included: those are fixed first.
Requests to change how the app looks, reads or feels (`ui-change`), feature requests and non-blocking annoyances are never
handed over and are never merged by the automation (see *Human review for UI changes* below). The issue text is **pinned** by a fingerprint: if it is edited after it was approved, the attempt
stops. Up to two rounds; each round is two jobs on **separate machines**:

1. **agent job** (the only place with the model credential): the model **reads and edits files in a plain copy of the
   tree** - without `.git`, with no shell and no network tool; reads outside the directory are denied. Changed files and
   its answer are scanned for credentials. Nothing the model wrote is built or run here.
2. **verify job** (no secret at all): applies the edits, classifies the diff (below) and - only if it is not `blocked` -
   builds and runs the unit tests. Failures are fed back to the next round.

So model-written code (a unit test, say) only ever runs on a machine that holds no credential, and cannot reach one through
the process table, the environment, the runner's file commands or the file system.
A separate job, with no model credential, re-validates the patch and opens a **draft PR** from an `agent/issue-N-*`
branch. Daily cap: `FIX_MAX_PER_DAY`.

### What an agent may change - the risk classes (`.github/agents/policy.mjs`)

| Class | Meaning | Examples |
|:--|:--|:--|
| **low** | small, local, ordinary | a crash fix, a label template, a translation, a one-file bug fix with a test |
| **high** | sensitive area or large | printing, file format, mail-merge, barcodes, kiosk, >120 changed lines or >5 files, deleting a file |
| **blocked** | a human must decide | any **user-interface change** (forms, stylesheets, visible text, widgets, layout), any path outside the allowed source directories (build files, scripts, `.github`, packaging, licenses, dotfiles, anything executable such as `.py`/`.js`, and the reporting/branding/attribution code), a diff longer than 60,000 characters (the council reads at most 70,000), binary files, executable bits, removed tests, >600 lines or >12 files, or **added lines that mention** process execution, networking, dynamic loading, environment variables, URLs or long encoded strings |

### Human review for UI changes

Autonomy is for problems that stop someone working. Anything about appearance or preference is a person's call, enforced in
three independent places:

1. **Triage**: category `ui-change` or `feature` is labelled `needs-human-review`, never `agent-ready`, never `blocks-user`,
   whatever the model says about severity.
2. **The diff**: any change that touches a `.ui`/`.qss`/`.css`/`.qrc` file, or adds user-visible text (`tr()`, tooltips,
   titles, labels), styling, widgets or layout, is classed **blocked** - a human must decide. The fix agent is also told not to
   do this and to answer `cannot_fix` instead.
3. **Merge time**: auto-merge requires the fixed issue to carry `blocks-user` and none of `ui-change`, `enhancement`,
   `needs-human-review`, `needs-human`. A maintainer who wants an agent PR merged without the label merges it by hand.

## 3. Council (`council.yml`)

Independent reviewers see the same diff, the issue (marked untrusted) and the deterministic classification, and answer
with a structured verdict. **Approval requires every reviewer that was asked to answer and approve** (any `request_changes`,
backdoor suspicion, scope complaint or blocking concern is a veto; a reviewer that errors out blocks approval - it cannot
quietly drop out of the vote), at least two to take part, and the combined risk not to be `blocked`. The result is a commit status **`Council`** on the exact reviewed commit - a later push invalidates it.

Members: `deepseek`, `glm` and `claude-haiku` from the pool below, each joining when its secret exists. A missing key only makes
approval harder (two must take part and all who take part must approve).

### Models and cost (`.github/agents/model-policy.mjs`)

Only four models can ever run, and the work is spread over them so no single budget or usage limit carries it all:

| id | model | paid by | used for |
|:--|:--|:--|:--|
| `deepseek` | OpenRouter `deepseek/deepseek-v4.1-flash-1017` | per token (about $0.03 / $0.20 per million) | triage, fixes, council |
| `glm` | OpenRouter `z-ai/glm-5.3-flash` | per token ($0.12 / $0.38 per million) | triage, fixes, council |
| `claude-sonnet` | Claude Sonnet | Claude subscription | fixes |
| `claude-haiku` | Claude Haiku | Claude subscription | triage, council |

- **Rotation**: the issue (or PR) number picks which member goes first, so work spreads evenly; if a member fails (usage limit,
  outage, no usable answer) the next one is tried. A fix retry round starts on the next member.
- **Allowlist**: exact names only. Aliases (`~...-latest`), `:batch` variants, other OpenRouter models, other Claude models (Opus),
  Gemini and anything else are refused before a request is made. `MODEL_POOL` can only narrow the pool, never add to it.
- **Price ceiling**: before every OpenRouter call the live per-token prices are read from OpenRouter and the call is refused if
  the model costs more than GLM 5.3 flash (or charges per request, search or image), or if the prices cannot be read.
- **Per-run budget**: a run is stopped as soon as the tokens it has streamed would cost more than its budget at the ceiling
  prices (triage $0.10, fix $0.50 per round, never more than $1 on a per-token model), even when the tool reports it as free.
- opencode runs with every tool off (triage, council) or with only file read/edit inside the work copy (fixes): no shell, no
  network, no plugins, no access outside the directory, and it sees only the OpenRouter key, never the GitHub token.
- Start **Model smoke test** (Actions tab) after changing models, secrets or the pinned CLI versions: it runs each member for
  real and checks the edit sandbox.

### Usage limits: wait, then most important first (`agent-dispatch.yml`)

A usage limit, rate limit or overloaded model is **not a failure**. When *every* model a stage may use refuses for that
reason (`limits.mjs` recognises the messages; a mix of limits and real failures stays a failure), the work is **parked**
instead of being rejected or sent to a human:

- the issue (fix, triage) or PR (council) gets the label `agent-queued` and one comment from the automation with a hidden
  marker: what it is, when to retry (read from the provider's message, kept between 10 minutes and 6 hours), and the
  fingerprint of the issue text it was approved with. The label alone does nothing: it counts only if the automation applied
  it last and a live marker from `github-actions[bot]` exists. Starting an item turns its marker into a *consumed* one, so
  re-adding the label later (anyone with triage rights can) replays nothing;
- a parked fix run ends as *cancelled*, so it does not use up the daily cap (`FIX_MAX_PER_DAY`), and a postponed council
  review sets the `Council` status to *pending*, never *failure* (it only postpones when nobody who answered objects);
- **every 30 minutes** `agent-dispatch.yml` looks at everything parked. While a parked fix or triage is still inside its wait
  nothing starts (every model was limited, so one probe is enough); a parked *council review* (usually Claude) holds only other
  reviews, because triage and fixes still run on the other models. Then it starts **one** item, the most important (and only if
  no fix, triage or review run is already going; an item whose issue was closed, edited or already has a PR is dropped):

  | | what | why first |
  |:--:|:--|:--|
  | 0 | fixes for problems that block a user (`blocks-user`) | someone cannot work |
  | 1 | council reviews | nearly finished work |
  | 2 | triage | cheap, and it decides everything else |
  | 3 | fixes a maintainer asked for (`agent-fix`) | an explicit request |
  | 4 | fixes triage marked `agent-ready` | |

  Older items go first within a tier. The automation level and the daily fix cap still apply;
- an item parked 12 times within a day is handed to a person (`needs-human`): a "limit" that never ends is something else. The
  count restarts for a different kind of work or a new day;
- a run that already did its first round before the limit hit stays counted against the daily cap; one that only waited does not.

## 4. Merge policy (`automerge.yml`)

Every 20 minutes and whenever the council approves, **one** PR is merged if all of this holds on its head commit:
opened by `github-actions[bot]` on an `agent/issue-*` branch based on the current `main` (a PR that fell behind needs a human); not a draft; `Council` = success; `CI OK` and `Lint OK` = success;
re-classified risk is not `blocked`; and the level/time rule allows it (low: level 3+ any time; high: level 4, 05:00-09:00
Central). It is a rebase merge pinned to the checked commit, and starts the edge build (merges made with the workflow
token do not trigger push workflows).

## 5. Release train (`release-train.yml`)

Stable releases are cut at **10:00 and 14:00 Central** (each slot is a 90-minute window because GitHub's scheduler can start
late; the cron runs at both UTC offsets and the script checks the real Chicago clock; a slot can be used once; if its release build dies on a passing infrastructure problem, such as a macOS runner that never starts, the train re-runs the failed jobs, up to three attempts, while the window is open), **at most two a day**, and **only if** something user-visible landed on main since the last release and
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

- Secrets used: `CLAUDE_OAUTH_TOKEN` (Claude Sonnet/Haiku), `OPENROUTER_API_KEY` (DeepSeek and GLM; either one alone is enough to
  run, both give the full pool), the
  relay's GitHub token (kept only as an encrypted value of the DigitalOcean Function, never in the repository).
- Repository setting needed: *Settings > Actions > General > Workflow permissions > Allow GitHub Actions to create and
  approve pull requests*.
- Branch protection: `main` requires `CI OK` and `Lint OK`; repository admins can bypass for maintenance.
- Labels used: `agent-queued` (parked for a usage limit), `from-app`, `needs-triage`, `triaged`, `needs-info`, `needs-human`, `agent-ready`, `blocks-user`, `ui-change`, `needs-human-review`, `agent-fix`, `agent-pr`,
  `council-approved`, `council-rejected`, `risk:low|high|blocked`, `area:*`.

## Setup checklist

Already in place: the `CLAUDE_OAUTH_TOKEN` secret, the labels, the `main` ruleset (`CI OK` + `Lint OK` required, admins may
bypass) and the setting that lets Actions create pull requests.

Still to do by the maintainer, once:

1. **Relay** - deployed as a DigitalOcean Function (`relay/README.md`). Set `REPORT_ENDPOINT` to its URL; builds made afterwards
   embed it. Until then the app offers only the browser flow.
2. **Level** - set `AUTOMATION_LEVEL` (see above). It was rehearsed end to end at level 3.
3. **Releases** - set `RELEASE_TRAIN` to `on` when you want the 10:00 / 14:00 schedule to start.
4. **Models** - `OPENROUTER_API_KEY` and `CLAUDE_OAUTH_TOKEN` are set; run **Model smoke test** once to confirm every member works.

## Rehearsal log

The pipeline was exercised on a planted documentation typo (issue #9): the issue was triaged and labelled `agent-ready`; the fix
agent proposed the one-word change; the first attempt was *blocked by the safety rules* (the verify job had staged the
workspace's cache directory - since fixed to stage only the model's files); the retry produced PR #11, which the council
(opus + haiku) approved as low risk; CI ran on the PR; the merge policy merged it and commented on the issue.
Things the rehearsal found and fixed: staging scope, CI runs held for approval on bot-opened PRs (required checks must belong to
the PR), a flaky runner (the merge policy now retries a failed CI/Lint once).
