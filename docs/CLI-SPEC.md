# glabelsneo CLI — Design Spec and Implementation Plan

Status: **Implemented.** This began as the design draft; §12 lists where the finished tool differs from it.
Branch: `feat/cli` (worktree `glabels-neo-cli`)

Approach: **evolve `glabels-batch-qt` into `glabelsneo`** rather than write a new tool. §9 lists what is kept,
what is rewritten, and the one hard constraint (the self-updater depends on the old executable's name).

## 1. Goal

Print a `.glabels` file from a terminal or from another program, and get a real label out of a real printer:

```
glabelsneo print jeans.glabels --field price=29.99 --field "name=Blue Jeans" --printer default
```

It must work **non-interactively** (scripts, services, POS integrations, CI) and **interactively** (a human at a
terminal who wants to be asked for what is missing). It must scale to large batches via **JSON**. It is the
foundation for later programmatic access (a local HTTP/IPC wrapper, language bindings, etc.), so its machine
interface (JSON in, JSON out, exit codes) is treated as a stable API from day one.

### Non-goals (v1)

- Editing or creating `.glabels` files from the CLI.
- Confirming that paper physically came out. Exit code 0 means *the job was handed to the OS spooler / printer
  driver*; Qt exposes no job-completion signal. This is documented, not hidden.
- A GUI print dialog (the GUI already has one).

## 2. What exists today (findings)

| Finding | Consequence |
|---|---|
| `glabels-batch-qt` (`glabels-batch/main.cpp`, 311 lines) already prints one file with merge input, `--printer`, `--output`, `--copies`, `--collate`, `--first`, `--outlines`, `--crop-marks`, `--reverse`, `-D var=value`. | **Decision: evolve it into `glabelsneo`.** Keep its print-setup logic, subsystem init, translators and option meanings; rewrite the parser, error handling and reporting (§9). |
| **`glabels-batch-qt` is load-bearing infrastructure, not just a user tool.** The self-updater (`update/InstallLayout.cpp`, `UpdateService.cpp`) treats `bin/glabels-batch-qt[.exe]` (Windows) / `glabels-batch-qt` (Linux) as a *required file* of every update package, and smoke-tests the new version by running it with `--Version` and requiring `GLABELS NEO` in the output. Release and CI workflows (`release.yml`, `ci.yml`), `scripts/package-linux.sh`, the Windows launcher and the updater unit tests all name it. **Releases v1.5.0–v1.5.2 already contain this updater.** | Installed copies run the *old* updater, which will reject any package lacking `glabels-batch-qt` as "incomplete". A plain rename would silently break updating for everyone already installed. So the rename must be staged (§9, "Rename without breaking updates"). |
| Settings and user data are keyed by `QCoreApplication::applicationName`. The batch tool sets its own name (`glabels-batch-qt`) while the GUI uses `Version::APPLICATION_NAME`. | **To verify in Phase 1:** whether the CLI currently reads a different settings store/user-template location than the GUI. The CLI should use the GUI's application name so both see the same user templates and preferences. |
| It logs through `qDebug()` unconditionally (noise on stderr, nothing structured). | New tool needs a real reporter. |
| A file that fails to parse still **exits 0**; other errors exit `-1` (255). | New tool needs defined exit codes. |
| `-D name=a=b` is rejected (it requires exactly one `=`). | Split on the *first* `=` only. |
| On Windows, `-` for stdin maps to `CON:` which is not a pipe. | Read stdin via the real stdin handle. |
| `glabels-qt` is built as `WIN32` (GUI subsystem) — it cannot write to a console. | The CLI must be a **separate console-subsystem executable**. It can't be a flag on the GUI binary. |
| `Variables::setVariables` already accepts arbitrary `name=value` and adds a STRING variable if undeclared. | `${anything}` can be satisfied from the CLI. |
| Substitution precedence (docs/SUBSTITUTION-FIELD-SPEC.md): **merge field beats user variable** of the same name. | Defines how `--field` and `--var` interact (§5). |
| `glabels/FillMerge` is an in-memory merge backend that takes `QList<Record>` — exactly what a JSON batch or `--field` set needs. But it lives in the GUI library. | Move to `backends/merge/` (as `MemoryMerge`); GUI and CLI share it. |
| `FillView::onPrintButtonClicked` routes Zebra printers through `ZplRenderer` and everything else through `QPrinter` + `PageRenderer::print`. `PageRenderer::print` has special handling for roll/thermal drivers that report no paper size. | If the CLI only did the plain QPrinter path, a Zebra label from the CLI would differ from the same label from the GUI. The routing must be shared (§9, Phase 0). **Not yet reviewed in depth: `ZplRenderer` also has `previewDialog`, so it needs splitting into UI and non-UI parts.** |
| `PageRenderer` has one global copy count and cannot do per-record copies. | Expand per-record copies into repeated records at load time. |
| Qt Core's `QJsonDocument` is already used (`BugReport`, updater). | No new dependency for JSON. |
| `Branding.hpp` says binary names deliberately remain `glabels*`. | `glabelsneo` is a **new additive** executable, so it breaks no identity string; flagged so it's a conscious choice. |

## 3. Command-line shape

```
glabelsneo [global options] <command> [command options] [arguments]
```

Commands:

| Command | Purpose | Phase |
|---|---|---|
| `print` | Print a label file (to a printer or a PDF). The main event. | 1 |
| `inspect` | Show the template, label size, merge source, and **fields/variables a file expects**. | 1 |
| `printers` | List printers; mark the default. | 1 |
| `preview` | _Built as `print --output label.png [--dpi N]`, not a separate command._ | done |
| `schema` | Print the JSON Schema for job files. | done |
| `version` / `--version` | Version info (`--verbose` = what `-V` does today). | 1 |
| `help [command]` / `--help` | Help. | 1 |

### 3.1 Global options

| Option | Meaning |
|---|---|
| `-h, --help` | Help (also per command). |
| `--version` | Short version. `glabelsneo version --verbose` for build details. |
| `-q, --quiet` | Suppress progress; errors only. |
| `-v, --verbose` | Diagnostics on stderr (replaces today's unconditional `qDebug`). Repeatable. |
| `--format text\|json` | Output format on **stdout**. Default `text`. `json` is the stable machine interface (§8). |
| `--no-input` | Never prompt, even on a TTY. Env: `GLABELSNEO_NO_INPUT=1`. |

Rule: **results go to stdout, diagnostics go to stderr.** Nothing else is ever written to stdout in `--format json`.

### 3.2 `glabelsneo print`

```
glabelsneo print [options] [FILE]
```

`FILE` is a `.glabels` path, or `-` for stdin. It may be omitted only when `--job` supplies it.

**Target (mutually exclusive):**

| Option | Meaning |
|---|---|
| `-p, --printer NAME` | Printer name. **Required: every `print` must name either `--printer` or `--output`** (no silent fallback, no environment variable), so a script can never hit the wrong printer by accident. `--printer default` is the explicit way to ask for the OS default. Matching is exact, then case-insensitive; **never fuzzy**. Ambiguous or unknown → exit 5. |
| `-o, --output FILE` | Write a PDF instead of printing (satisfies the "must name a target" rule). `-` = stdout. No printer involved. |

Neither given → exit 2 (usage error), except in interactive mode on a terminal, which asks. Giving both → exit 2.

**Data — what goes in the fields:**

| Option | Meaning |
|---|---|
| `-F, --field NAME=VALUE` | Set merge field `NAME` for the label. Repeatable. Split on the **first** `=`. Value may be empty. |
| `--var NAME=VALUE` | Set the initial value of a *user variable* (serial counters etc.). Repeatable. |
| `--data SOURCE` | Records source for batches: a file path or `-` for stdin. Format is chosen by `--data-format`, else by extension: `.json`, `.jsonl`/`.ndjson`, `.csv`, `.tsv`, and the existing colon/semicolon formats (name them with `--data-format`). |
| `--data-format FMT` | `json`, `jsonl`, `csv`, `tsv`, `colon`, `semicolon`; add `-nokeys` for files without a header row. Required when reading `--data -`. |
| `--job FILE` | Run a **job document** (JSON, §6). `-` = stdin. Supplies file, printer, options, and records in one document; command-line options override it. |

**Quantity and layout (same meanings as `glabels-batch-qt`, so existing knowledge transfers):**

| Option | Meaning |
|---|---|
| `-n, --copies N` | Copies of each record (default 1). |
| `--sheets N` | Full sheets of a simple (non-merge) project. |
| `--first N` | Starting label position on the first sheet (default 1). |
| `--collate` | Collate merge copies. |
| `--group` | Start each merge group on a new page. |
| `--outlines` / `--crop-marks` / `--reverse` | As today. |

**Behavior:**

| Option | Meaning |
|---|---|
| `-i, --interactive` | Force prompts: ask for every field (defaults shown), then confirm. |
| `-y, --yes` | Skip the final confirmation. |
| `--dry-run` | Do everything *except* print: resolve printer, validate every field, compute label/page counts. Safe for programs to call. |
| `--force-fields` | Make every `--field` value override the data file's value for the same key (default: the data file wins). One switch for the whole run. |
| `--strict` | Turn field warnings (below) into errors (exit 4). Off by default. |
| `--max-labels N` | Safety cap (default 1000). Exceeding it fails (non-interactive) or asks (interactive). `0` = no cap. |
| `--continue-on-error` | In multi-job runs, keep going after a failed job. Default: stop at the first failure. |

#### Field semantics (important)

- `--field` records are merge records, so `${name}`, `${name:%.2f}`, `${name:=default}` and `${name:n}` all work exactly as in the file today.
- If the file declares a **user variable** with the same name, the field wins (existing precedence).
- If the file has its **own merge source** and no `--field`/`--data` is given: print from that source (today's `-i` behavior). `--data` replaces the file's source. `--field` alongside `--data` acts as per-run constants merged into every record; if both set the same key, **the data file's value wins**, and `--force-fields` flips that so `--field` wins.
- **Lenient by default:** a field the file references but nobody supplied **prints blank** (or uses its `${x:=default}`); a supplied field the file never references is ignored. Neither is an error.
- **But never silent.** Each such case emits a **warning** on stderr (and in `warnings[]` of the JSON result), e.g. `field 'prise' is not used by the template; did you mean 'price'?` and `template field 'price' was not supplied; it will print blank`. A typo should not cost a label unnoticed. `--strict` makes warnings fatal for callers who want that.
- Interactive mode *asks* for missing fields instead of leaving them blank.

### 3.3 `glabelsneo inspect FILE`

Prints (text, or `--format json`): template brand/part, label size, label shape, labels per sheet, merge source,
declared user variables (name, type, increment, initial value), and **the field names referenced by text and
barcode objects**, each marked `required` or `has-default`.

This is how a programmatic caller discovers what to send, and how interactive mode knows what to prompt for.

```
$ glabelsneo inspect jeans.glabels --format json
{"template":{"brand":"Brother","part":"DK-1201","widthPt":82.09,"heightPt":254.64},
 "labelsPerSheet":1,
 "fields":[{"name":"price","required":true},{"name":"name","required":true}],
 "variables":[],
 "merge":{"type":"none","source":""}}
```

### 3.4 `glabelsneo printers`

Lists installed printers: name, `default` flag, `kind` (`system` / `zebra-zpl`), and state when the platform
reports it. `--format json` for programs. `--check NAME` exits 0/5 depending on whether it resolves.

## 4. Interactive mode

**Interactive = terminal prompts.** (If you meant "open the GUI print dialog", say so; that is a different and
much smaller feature.)

Prompts happen **only when stdin is a TTY and `--no-input` is not set**. Scripts never hang waiting for input.

1. **Automatic:** running `print` on a TTY with required fields missing, or no `--printer` and several printers installed, prompts for exactly the missing items.
2. **Forced (`-i`):** prompts for *every* field, each with its current value as the default (press Enter to keep).
3. **Confirmation** before sending (skipped by `--yes`):
   ```
   Print 3 copies of "jeans.glabels" (Brother DK-1201, 82×255 pt)
   to "Brother QL-800" ?  price=29.99  name=Blue Jeans   [Y/n]
   ```
4. After printing, optionally "Print another with different values? [y/N]" with previous values as defaults (shop-counter workflow). *Phase 2 stretch.*

Ctrl-C at any prompt exits 130 having printed nothing.

## 5. Why not `-field1 29.99 -field2 Blue Jeans`

I recommend **against** that exact syntax; the rest of this spec replaces it with `--field name=value`.

1. **`Blue Jeans` can't work in any shell.** The shell splits on spaces: the program receives `-field2`, `Blue`, `Jeans` and cannot tell whether `Jeans` is a value, a second value, or a stray argument. It *must* be quoted (`-field2 "Blue Jeans"`) regardless of what we design.
2. **Field names are open-ended.** Qt's `QCommandLineParser` (and every standard parser) needs options declared up front. Accepting arbitrary `-whatever` means a hand-written parser, no `--help` for fields, and typos like `-pirce` silently becoming field names.
3. **Collisions.** A file with a field called `printer` or `copies` makes `-printer` ambiguous.
4. **Single-dash long options** (`-printer`) clash with grouped short flags (`-pn`) and with Qt's own handling.

`--field NAME=VALUE` is unambiguous, self-documenting, handles spaces/`=`/unicode with ordinary quoting, and is
what `docker -e`, `git -c`, and `kubectl --set` do. If you strongly want the shorter form, a cheap compromise is
**trailing key=value arguments**: `glabelsneo print jeans.glabels price=29.99 "name=Blue Jeans" -p default`.
That's still one extra rule, so it is listed under "Still open" (§11), not planned.

## 6. JSON for batches

Three input forms, one internal representation (`JobSpec`, §7), so behavior is identical across them.

### 6.1 Records — `--data records.json` / `.jsonl`

For "one template, many labels". Implemented as a **new merge backend** (`backends/merge/Json`, `JsonLines`)
registered in `merge::Factory` — which means the **GUI's merge dialog gains JSON support for free.**

Array form:
```json
[
  { "price": "29.99", "name": "Blue Jeans", "_copies": 2 },
  { "price": "49.50", "name": "Corduroy Pants" }
]
```
JSON Lines form (for streaming from another program, no need to hold the whole batch in memory):
```
{"price":"29.99","name":"Blue Jeans"}
{"price":"49.50","name":"Corduroy Pants"}
```

Rules:
- Values are coerced to strings: numbers keep their JSON text (`29.99`, not `29.990000001`), booleans → `true`/`false`, `null` → empty. **Nested objects/arrays are an error** (reported with their JSON path) — no silent flattening.
- `_copies` (reserved, optional, integer ≥ 1): per-record copies; expanded to repeated records at load time.
- Keys beginning `_` are reserved for CLI metadata and never become fields.
- Input must be UTF-8. A UTF-8 BOM is stripped. A UTF-16 BOM (what Windows PowerShell 5.1 emits for `>` and some pipes) gets a clear error naming the fix, not mojibake.

### 6.2 Job document — `--job job.json`

For programs: one document carries everything, so the caller needs no other flags.

```json
{
  "version": 1,
  "jobs": [
    {
      "id": "order-1041-line-1",
      "file": "templates/jeans.glabels",
      "printer": "Brother QL-800",
      "options": { "copies": 1, "first": 1, "collate": false, "outlines": false },
      "variables": { "batch": "A7" },
      "defaults": { "currency": "USD" },
      "labels": [
        { "fields": { "price": "29.99", "name": "Blue Jeans" }, "copies": 2 },
        { "fields": { "price": "49.50", "name": "Corduroy Pants" } }
      ]
    },
    {
      "id": "order-1041-shipping",
      "file": "templates/ship.glabels",
      "output": "out/ship-1041.pdf",
      "data": "ship-1041.jsonl"
    }
  ]
}
```

- A document with `labels` (or `data`) at top level and no `jobs` is a one-job shorthand.
- `file` paths resolve relative to the **job document's directory** (so a job file is portable); for stdin, relative to the working directory.
- Each job must end up with exactly one of `printer` or `output` — from the job itself or from the command line (`--printer`/`--output` applied to the whole run). Neither or both → exit 2.
- `defaults` are merged under every label's `fields`; the label's own value wins.
- `id` is optional, echoed back verbatim in results so callers can correlate.
- Unknown keys are an error (catches typos); extension happens through `"version"`, not silent tolerance. `glabelsneo schema` emits the JSON Schema (Phase 4).

### 6.3 Behavior at scale

- **Preflight, then print.** Parse the whole document, open every template, resolve every printer, and check the shape of every record **before sending anything**. Any error (bad file, bad JSON, unknown printer) aborts with nothing printed; field mismatches are collected as warnings and reported up front. A half-printed batch of label stock is the worst outcome.
- `--max-labels` caps accidental floods (default 1000).
- JSONL is streamed; array JSON is loaded (documented limit: use JSONL beyond ~100k records).
- Multi-job runs print sequentially in document order.

## 7. Output and exit codes

### 7.1 Results (`--format json`)


```json
{
  "ok": true,
  "dryRun": false,
  "warnings": [ { "code": "field.unused", "job": "order-1041-line-1", "label": 1,
                  "message": "Field 'prise' is not used by the template; did you mean 'price'?" } ],
  "jobs": [
    { "id": "order-1041-line-1", "status": "printed",
      "file": "templates/jeans.glabels", "printer": "Brother QL-800",
      "labels": 3, "pages": 3 },
    { "id": "order-1041-shipping", "status": "printed",
      "output": "out/ship-1041.pdf", "labels": 12, "pages": 1 }
  ],
  "errors": []
}
```
Failure:
```json
{ "ok": false, "jobs": [],
  "errors": [ { "code": "field.missing", "job": "order-1041-line-1", "label": 1,
                "message": "Field 'price' is used by the template but not supplied." } ] }
```
`code` strings are stable API; `message` is human text and is **not** translated in JSON mode.
Text mode prints the same information in prose.

### 7.2 Exit codes

| Code | Meaning |
|---:|---|
| 0 | Success (handed to spooler / PDF written). `--dry-run`: all checks passed. |
| 1 | Unexpected internal error. |
| 2 | Usage error (bad/unknown/conflicting options). |
| 3 | Input file error (not found, unreadable, not a valid `.glabels`). |
| 4 | Data error (malformed JSON/records, exceeds `--max-labels`; also field warnings when `--strict`). Missing/unknown fields alone are warnings, not errors. |
| 5 | Printer error (not found, ambiguous, offline, spool failure, no write access to `--output`). |
| 6 | Partial failure (`--continue-on-error`: some jobs printed, some failed). |
| 130 | Interrupted. |

## 8. Platform notes

- **Executable:** `glabelsneo` (`glabelsneo.exe`), console subsystem, links `Model` + `backends` only — **not** the Widgets GUI library, and `QGuiApplication` rather than `QApplication`.
- **Linux headless:** Qt needs a platform plugin for fonts. If `DISPLAY`/`WAYLAND_DISPLAY` are both unset the CLI selects `QT_QPA_PLATFORM=offscreen` itself. That removes the `xvfb-run` workaround in today's man page.
- **Windows:** args via `QCoreApplication::arguments()` (wide, so unicode field values survive); stdin/stdout set to UTF-8; BOM handling per §6.1. The NSIS installer already has `CPACK_NSIS_MODIFY_PATH ON`, so `glabelsneo` must land in the same `bin` directory to be on PATH. Packaging scripts need to include it.
- **macOS:** ship inside the app bundle plus a documented symlink/`PATH` instruction.
- **Locale:** help text and prompts are translatable (`tr`), JSON keys/codes never.

## 9. Architecture

The directory `glabels-batch/` is renamed (`git mv`, history kept) to `glabelsneo/`, matching `glabels-fill/`.

```
glabelsneo/               was glabels-batch/; target: glabelsneo
  main.cpp                app setup, platform selection, dispatch (slimmed from today's 311 lines)
  Commands/               PrintCommand, InspectCommand, PrintersCommand (+ later Preview, Schema)
  JobSpec.{hpp,cpp}       the ONE internal representation of "what to print"
  JobLoader.{hpp,cpp}     flags -> JobSpec; --job JSON -> JobSpec[]; validation
  FieldResolver.{hpp,cpp} required/unknown/did-you-mean, precedence rules
  Prompter.{hpp,cpp}      TTY detection, prompts, confirmation (plain stdio, no readline dep)
  Reporter.{hpp,cpp}      text/json results, error codes -> exit codes
```

Design rule: **every entry point builds a `JobSpec` and calls one executor.** Flags, `--job`, and `--data` can't
drift apart, and a future HTTP/IPC front end reuses the executor unchanged.

Library work this depends on (Phase 0):

1. **Shared print service** extracted from `FillView`/`PrintView` into a non-GUI library: printer resolution, Zebra→ZPL routing, PDF output, and the `PageRenderer::print` roll-printer handling. GUI and CLI both call it; behavior stays identical by construction. Requires splitting `ZplRenderer`'s UI (`previewDialog`) from its transport/render code.
2. **`MemoryMerge`**: `FillMerge` moved to `backends/merge/` (GUI keeps working through the same type).
3. **Field discovery** on the model: enumerate `${…}` references (with their `:=default` presence) across text objects and barcode data. Today only `RawText::hasPlaceHolders()` exists. `inspect`, the field warnings (and `--strict`), and interactive prompting all need it.
4. **JSON / JSONL merge backends** (also a GUI feature).
5. A `PageRenderer` consumer of per-record copies is **not** needed: copies expand at load time.

### What is reused from `glabels-batch-qt`, and what is not

| From today's `main.cpp` | Fate |
|---|---|
| GPL header and Jaye Evins copyright | **Kept** (it's a derivative; `scripts/check-license-headers.sh` must keep passing). |
| Subsystem init order: `Settings::init`, `Db::init`, `merge::Factory::init`, `barcode::Backends::init` | **Kept** verbatim. |
| Qt/glabels/templates translator setup | **Kept**, moved into a helper. GUI-only `BrandTranslator` stays out (the CLI doesn't link the GUI library). |
| `XmlLabelParser::readFile` → `Model`; `variables().setVariables(...)` | **Kept** (`--var`). |
| Renderer configuration: simple project vs merge project (`setNCopies`, `setStartItem`, `setIsCollated`, `setAreGroupsContiguous`, `--sheets` vs `--copies` rules) | **Kept as the core of `PrintCommand`'s executor**, lifted out of `main()` unchanged in behavior, then fed by `MemoryMerge` for `--field`/`--data`. |
| `QPrinter` setup (HighResolution, Color) and `PageRenderer::print` | **Kept**, wrapped by the shared print service (adds Zebra/ZPL routing, printer resolution). |
| Option meanings: `-c`, `-s`, `-f`, `-a`, `-g`, `-l`, `-m`, `-r`, `-o`, `-p` | **Kept**. Existing knowledge transfers. |
| `-V/--Version` detailed version output | **Kept exactly**, including the `GLABELS NEO` text the updater matches. |
| `QCommandLineParser` single flat option set | **Rewritten**: top-level command dispatch, then a per-command parser. |
| `qDebug()` progress, `return -1` / silent `return 0` on bad file | **Rewritten**: `Reporter` + the exit-code table (§7.2). |
| `-D var=value` (rejects `=` in value) | **Replaced** by `--var`, split on first `=`. |
| `STDIN_FILENAME = "CON:"` on Windows | **Replaced** by real stdin handling. |
| `setApplicationName("glabels-batch-qt")` | **Changed** to the GUI's application name (verify, see §2). |
| Linux "needs X/Wayland" limitation | **Removed** by selecting the `offscreen` platform automatically. |

Rough estimate: ~40% of today's lines survive as logic; the shape (one big `main()`) does not.

### Rename without breaking updates

Installed copies (v1.5.x) carry an updater that **requires** `glabels-batch-qt` in every update package and runs
`glabels-batch-qt --Version` expecting `GLABELS NEO`. Therefore:

1. **Release N (the rename):** the real tool is `glabelsneo`. The package **also ships `glabels-batch-qt`**, built from the same sources as a *legacy-mode* executable: it parses the old flags (`-D`, `-i`, and so on), keeps the old semantics where the new rules would break scripts (notably it does **not** require `--printer`), answers `--Version` identically, and prints a one-line deprecation notice to stderr on normal use. In the same release the updater's `smokeRel` changes to `glabelsneo`, so installs that reach N get a new updater.
2. **Release N+1:** every install that updated through N now has the new updater, so `glabels-batch-qt` is dropped from packages and CMake. Anyone who skipped N updates manually.
3. If release N+1 would drop it before most installs have moved past N, hold the shim another release; the cost of keeping it is tiny, the cost of a stranded install is not.

This touches build, packaging and updater files — **`update/`, `scripts/`, `.github/`, `tools/`, CMake** — which the
automation policy (`.github/agents/policy.mjs`) deliberately keeps away from the auto-fix agents. These changes
need a human review. The same policy file lists `^glabels-batch\//` as an agent-editable path; it must gain
`^glabelsneo\//` in the same change.

Files that name `glabels-batch-qt` today and change in the rename: `CMakeLists.txt`, `glabels-batch/CMakeLists.txt`,
`.github/workflows/ci.yml`, `.github/workflows/release.yml`, `.github/agents/policy.mjs`,
`.github/agents/fix-step.mjs`, `scripts/package-linux.sh`, `tools/windows-launcher/*.ps1`,
`update/InstallLayout.cpp`, update unit tests (`TestInstallLayout.cpp`, `fake_batch.cpp`), `user-docs/CMakeLists.txt`
and `user-docs/man/`, `docs/AUTO-UPDATE.md`, and `docs/TODO.md`.

Also: `Branding.hpp`'s note that "binary names remain `glabels*`" gets an explicit exception for `glabelsneo`.

Flag mapping for the legacy executable and for migration notes: `-p`→`--printer`, `-o`→`--output`, `-i`→`--data`,
`-c`→`--copies`, `-D`→`--var`, `-f`→`--first`, the rest unchanged. Behavior changes called out in release notes
(exit codes, stdout/stderr split, `--printer` now required).

## 10. Phases

| Phase | Deliverable | Notes |
|---|---|---|
| **0 — Foundations** | Shared print service; `MemoryMerge`; field discovery on `Model`. GUI behavior unchanged. | The risky part. Zebra/ZPL split is the unknown. |
| **1a — Rename, nothing else** | `git mv glabels-batch glabelsneo`; target `glabelsneo`; legacy-mode `glabels-batch-qt` built from the same sources; updater `smokeRel` → `glabelsneo`; CI/release/packaging/launcher/policy updated; `Branding.hpp` note. **Behavior identical to today.** | Own PR, human-reviewed (touches updater + packaging). Verify with a real update from a v1.5.x install. |
| **1b — Core, non-interactive** | `glabelsneo print` (single label, `--field`, `--var`, `--printer`/`--output` required, quantity/layout flags, `--dry-run`, warnings + `--strict`), `inspect`, `printers`, `--format json`, exit codes. | Delivers your example command. |
| **2 — Interactive** | Prompter, `-i`, auto-prompt for missing fields/printer, confirmation, `--yes`/`--no-input`. | Pure addition on the Phase 1 executor. |
| **3 — JSON batches** | JSON/JSONL merge backends, `--data`, `--job`, multi-job, preflight, `--max-labels`, `--continue-on-error`. | Also adds JSON merge to the GUI. |
| **4 — Polish** | `preview` (PNG), `schema`, shell completions, deprecation shim, man page + README + JSON Schema in docs. | |

### Testing

- Qt Test unit tests (the project already uses them): arg→`JobSpec`, JSON/JSONL parsing incl. BOM and nested-value errors, field resolution/did-you-mean, exit-code mapping, per-record-copy expansion.
- **No printer needed in CI:** integration tests run `glabelsneo print … --output x.pdf` and `--dry-run` under `offscreen` against `test-data/*.glabels`, asserting page/label counts and exit codes. Real-printer behavior is a manual checklist (one regular printer, one roll printer, one Zebra).
- Machine-interface contract tests lock the `--format json` shape and error `code` strings.

> The dev machine currently has no C++ toolchain, so these run in CI; local iteration needs the portable Qt toolchain described in the team notes.

## 11. Decisions

| # | Decision |
|---|---|
| 1 | Interactive = **terminal prompts**, not a dialog. |
| 2 | Data file wins over `--field`; `--force-fields` reverses it. |
| 3 | `--printer` (or `--output`) is **required** on every `print`; no env-var or silent default. `--printer default` is explicit. |
| 4 | Unknown / unfilled fields **print blank**, with a stderr/JSON **warning**; `--strict` makes them errors. |
| 5 | Zebra printers route through ZPL exactly like the GUI. |
| 6 | New binary name **`glabelsneo`**; `Branding.hpp` is updated to allow it. |
| 7 | **Evolve `glabels-batch-qt`** into `glabelsneo` instead of starting fresh; legacy executable shipped for one release because the updater depends on it (§9). |
| 8 | Field syntax is `--field NAME=VALUE` (the `-field1 value` form is not supported, §5). |

### Still open

1. **Trailing `key=value` sugar** (`glabelsneo print f.glabels price=29.99 -p default`) — add or skip? Default: skip.
2. **Legacy shim lifetime** — drop in N+1 (default), or hold until update telemetry/anecdotes say installs have moved on?
3. **Directory name** — `glabelsneo/` (default) vs keeping `glabels-batch/` to avoid touching the automation policy file.


## 12. As built: where the finished tool differs from this draft

The tool is `glabelsneo/` (the former `glabels-batch/`), with a shared `printing/` library. Differences from the
text above, all deliberate:

- **Preview is `print --output label.png`**, with `--dpi N`. One page keeps the exact name; several become
  `label-1.png`, `label-2.png`, and so on. It goes through the same pipeline as a real print (fields, batches, options),
  so it is also how the tests and CI look at output without a printer. There is no separate `preview` command.
- **`schema`** prints the JSON Schema of job documents. A test keeps its property lists in step with the loader.
- **`--driver auto|system|zpl`** was added. Zebra detection is the GUI's name-based heuristic, which can misfire, so a
  script can override it. `printers` shows which printers `auto` would drive with ZPL.
- **`--color` / `NO_COLOR` were dropped** (the tool prints no color). **`.xlsx` was dropped** from `--data`: Excel
  reading lives in the GUI's Fill page, not in the merge backends the tool shares.
- **Relative paths in a job document** (`file`, `data` and `output`) all resolve against the document's directory.
- **Prompts are decided by a real terminal check.** `_isatty` is true for the NUL device on Windows (so `< NUL`
  looked interactive); the tool asks for a real console, or an MSYS/mintty pseudo-terminal. `GLABELSNEO_ASSUME_TTY=1`
  is an undocumented test seam. Prompts also require that standard input is not already used for data, and are
  off under `--format json` unless `-i` is given.
- **A PDF on standard output** (`-o -`) keeps standard output byte-clean: the human summary goes to standard error,
  and `--format json` is refused with it (exit 2).
- **The Zebra path renders per page.** The GUI's `ZplRenderer::renderZpl` draws one label with an *empty* merge record,
  so the Fill page's Zebra button cannot print per-record data. The command line renders each page through
  `PageRenderer` and sends one ZPL block per page. The GUI path was left as it was (it now shares the ZPL packing and
  transports with the tool).
- **`glabels-batch-qt` stays for one release** as a separate legacy executable built from the original source
  (`legacy_main.cpp`), with a deprecation notice on standard error when it prints. `--Version` stays silent and keeps
  answering `GLABELS NEO`, because installed copies' self-updater runs exactly that. The updater now smoke-tests
  `glabelsneo` instead.
- **Application identity:** the tool uses the GUI's application name, so it shares preferences, user templates and Zebra
  connection profiles with the GUI (the old batch tool had its own).
- **Not done:** shell completions; macOS packaging (the disk image has never included a command line tool, and bundle
  rpaths could not be tested without a Mac).
