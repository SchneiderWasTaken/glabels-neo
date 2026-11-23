# Report relay

A tiny serverless function that lets **Help > Report a Problem** in gLabels Neo file a GitHub issue for a user who has
no GitHub account, without the app holding any credential. It runs as a **DigitalOcean Function** (free tier, no server
to look after).

```
 gLabels Neo  --(1) GET  /v1/challenge --------------->  relay
              <-(2) signed proof-of-work challenge-----
              --(3) POST /v1/reports (report + solution)->  relay --(4) create issue--> GitHub
              <-(5) { number, url }---------------------
```

The relay holds the only GitHub credential, a **fine-grained token that can only read and write issues in this one
repository**. It is never given to the app. It is stored only as an encrypted environment value of the function.

## What it defends against

| Threat | Defence |
|:--|:--|
| Someone extracts a token from the app | there is no token in the app |
| Scripted spam | a signed, expiring proof-of-work challenge (about a second of CPU), a User-Agent check, per-reporter hourly/daily limits and a global daily circuit breaker |
| Replayed challenges and bursts | the relay has no storage, so the **issue tracker is its state**: every report it files carries a hidden marker (hash of the sender, challenge id). New reports are checked against the recent ones, and a few seconds after filing, each report looks again and **withdraws itself** (closes its own issue, label `relay-rejected`) if an earlier one made it a duplicate or pushed it over a limit |
| A report that tries to ping people, forge markers, or inject Markdown | `@` mentions, HTML comments and tags, images, links and cross-repo references are neutralised; code fences cannot be closed from inside |
| A client choosing labels such as `agent-fix` | labels are fixed by the relay (`bug`, `from-app`, `needs-triage`) |
| Oversized input | request, field and attachment size limits |
| Abuse you want to stop *now* | redeploy with `ENABLED: "false"` (the app falls back to the browser flow) |

Everything a report contains is still **untrusted text** and is treated that way by the triage automation
(see `docs/AUTOMATION.md`).

## One-time setup

You need `doctl` signed in (`doctl auth init`) and a fine-grained GitHub token. Windows needs Developer Mode on for
`doctl serverless install` (it creates symbolic links).

1. **GitHub token.** Create a fine-grained personal access token: resource owner = you, **only select repositories >
   glabels-neo**, repository permission **Issues: Read and write** and nothing else, expiry 1 year. Save it to a file
   that is not in the repository.
2. **doctl serverless.**
   ```
   doctl serverless install
   doctl serverless namespaces create --label glabels-neo-relay --region sfo3
   doctl serverless connect
   ```
3. **Deploy.**
   ```
   powershell -ExecutionPolicy Bypass -File relay\do\deploy.ps1 -TokenFile <path to the token file>
   ```
   It stages the sources, generates the challenge-signing secret once (kept in `%LOCALAPPDATA%\gLabelsNeo`), passes both
   secrets to the function through a temporary env file that is deleted afterwards, and prints the function URL.
4. **Point the app at it.** Set the repository variable `REPORT_ENDPOINT` to that URL. Builds made after that embed it;
   until then the dialog uses the browser fallback.

Check it: `curl <url>/v1/health` returns `{"ok":true,"enabled":true}`.

## Operating it

| I want to... | Do this |
|:--|:--|
| Pause reports | set `ENABLED: "false"` in `do/project.yml` and redeploy |
| Tighten / loosen limits | `LIMIT_PER_HOUR`, `LIMIT_PER_DAY`, `LIMIT_GLOBAL_PER_DAY`, `POW_BITS` in `do/project.yml`, redeploy |
| Rotate the GitHub token | create a new one, run `deploy.ps1 -TokenFile ...` again |
| Rotate the signing secret | delete `%LOCALAPPDATA%\gLabelsNeo\relay-challenge-secret.txt`, redeploy (open challenges expire in 10 minutes) |
| See what is happening | `doctl serverless activations logs --function relay/report --follow` (logs contain no report text) |

## Develop

```
cd relay
npm test            # node --test, no dependencies (also runs the DigitalOcean adapter)
```

The core (`src/`) uses only `fetch` and WebCrypto, so it also runs on Node, Cloudflare Workers and similar.
`do/` holds the DigitalOcean wrapper: `project.yml`, the adapter (`packages/relay/report/adapter.mjs`), `stage.mjs`
(copies `src/` next to the entry point) and `deploy.ps1`.

## Report format

Issues are created with a hidden marker (`<!-- glabels-neo-report:v1 -->`), a hidden tracking comment, a
`### What happened?` section (the user's words) and an `### Environment` code block. A label file or log, if the user
chose to include one, is posted as a collapsed comment. The format matches the bug-report issue form, so triage handles
app reports and web reports the same way.
