# Report relay

A tiny Cloudflare Worker that lets the **Help > Report a Problem** dialog in gLabels Neo file a GitHub issue
for a user who has no GitHub account and without the app holding any credentials.

```
 gLabels Neo  --(1) GET /v1/challenge------------------>  relay
              <-(2) signed proof-of-work challenge-----
              --(3) POST /v1/reports (report + solution)->  relay --(4) create issue--> GitHub
              <-(5) { number, url }---------------------
```

The relay holds the only GitHub credential, and that credential can only **create issues and comments in this
repository**. It is never given to the app.

## What it defends against

| Threat | Defence |
|:--|:--|
| Someone extracts a token from the app | there is no token in the app |
| Scripted spam | a signed, single-use, expiring proof-of-work challenge (about a second of CPU), a User-Agent check, per-reporter hourly/daily limits and a global daily circuit breaker, all enforced atomically by a Durable Object (a KV fallback is used only in local development) |
| A report that tries to ping people or forge machine-readable markers | `@` mentions are broken, HTML comments and tags are escaped, code fences cannot be closed from inside |
| A client choosing labels such as `agent-fix` | labels are fixed by the relay (`bug`, `from-app`, `needs-triage`) |
| Oversized input | request, field and attachment size limits |
| Abuse you want to stop *now* | set `ENABLED = "false"` in `wrangler.toml` (or the variable in the dashboard); the app falls back to the browser flow |

Everything a report contains is still **untrusted text** and is treated that way by the triage automation
(see `docs/AUTOMATION.md`).

## One-time setup

You need a Cloudflare account (the free plan is enough) and about ten minutes.

### 1. Let the relay file issues

Pick **one**:

**A. GitHub App (recommended).** Issues appear as `<app name>[bot]`, the credential never expires, and it is
scoped to one repository.

1. GitHub > Settings > Developer settings > GitHub Apps > **New GitHub App**.
2. Name: `gLabels Neo Reports`. Homepage URL: this repository. Untick **Webhook > Active**.
3. Repository permissions: **Issues: Read and write**. Nothing else.
4. "Where can this GitHub App be installed?": **Only on this account**.
5. Create the app, note its **App ID**, then **Generate a private key** (a `.pem` file).
6. **Install App** on this repository only; the number at the end of the installation URL is the
   **installation id**.
7. Add three Actions secrets in this repository: `RELAY_GITHUB_APP_ID`, `RELAY_GITHUB_APP_INSTALLATION_ID`,
   `RELAY_GITHUB_APP_PRIVATE_KEY` (paste the whole PEM; PKCS#1 and PKCS#8 are both accepted).

**B. Fine-grained token (quicker).** Create a fine-grained personal access token limited to this repository
with **Issues: Read and write**, and add it as the Actions secret `RELAY_GITHUB_TOKEN`. Issues will be
authored by your account, and the token expires (renew it).

### 2. Cloudflare

1. Create an API token with the **Edit Cloudflare Workers** template; note your **Account ID**.
2. Add Actions secrets `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID`.

### 3. Deploy

1. Set the repository variable `RELAY_DEPLOY` to `true`.
2. Run **Actions > Deploy report relay** (it also runs on every change under `relay/`).
   It deploys the worker, provisions the rate-limit KV namespace, and sets the worker's secrets.
3. Take the worker URL it prints (`https://glabels-neo-report.<your-subdomain>.workers.dev`) and set the
   repository variable `REPORT_ENDPOINT` to it.
4. The next release build embeds the URL. Until then the dialog uses the browser fallback.

Check it: `curl https://<worker-url>/v1/health` should return `{"ok":true,"enabled":true}`.

## Operating it

| I want to... | Do this |
|:--|:--|
| Pause reports | set `ENABLED = "false"` and redeploy (the app falls back to the browser) |
| Tighten / loosen limits | `LIMIT_PER_HOUR`, `LIMIT_PER_DAY`, `LIMIT_GLOBAL_PER_DAY`, `POW_BITS` in `wrangler.toml` |
| Rotate credentials | replace the Actions secrets and re-run the deploy workflow |
| See what is happening | `npx wrangler tail` (logs contain no report text) |

## Develop

```
cd relay
npm test            # node --test, no dependencies
npm run dev         # wrangler dev (needs `npm ci`)
```

The code uses only `fetch` and WebCrypto, so the same handler also runs on Node (for example on a VPS) behind
any HTTP adapter.

## Report format

Issues are created with a hidden marker (`<!-- glabels-neo-report:v1 -->`), a `### What happened?` section
(the user's words) and an `### Environment` code block. A label file or log, if the user chose to include one,
is posted as a collapsed comment. The format matches the bug-report issue form, so triage handles app reports
and web reports the same way.
