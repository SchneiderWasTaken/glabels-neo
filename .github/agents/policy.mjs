// Pure policy functions for the automation pipeline.  No I/O, so they can be unit-tested exhaustively.
//
// The rules here are the safety boundary between "an agent wrote this" and "this reaches users":
//   - which paths an agent may change at all,
//   - which patterns force a human to look,
//   - when (Chicago time) risky changes may be merged,
//   - when a release may be cut.

import { parseDiff } from './diff-lib.mjs';

export const TIME_ZONE = 'America/Chicago';

// ------------------------------------------------------------------------------------------ levels

/**
 * AUTOMATION_LEVEL (repository variable, default 1):
 *   0  off
 *   1  triage only: label and comment on new issues
 *   2  + agents draft fixes when a maintainer applies `agent-fix` (humans merge)
 *   3  + agents pick up issues triage marks `agent-ready`; council-approved LOW risk PRs auto-merge
 *   4  + council-approved HIGH risk PRs auto-merge, inside the early-morning window only
 * `blocked` changes never auto-merge at any level.
 */
export function parseLevel(value) {
  const n = Number.parseInt(String(value ?? '').trim(), 10);
  if (!Number.isInteger(n) || n < 0) return 1;
  return Math.min(n, 4);
}

// ------------------------------------------------------------------------------------------ paths

// Paths an agent may change.  Anything else is out of scope and blocks auto-merge.
const AGENT_ALLOWED = [
  /^glabels\//, /^model\//, /^glbarcode\//, /^glabels-batch\//, /^glabels-fill\//, /^backends\//,
  /^templates\//, /^translations\//, /^test-data\//, /^user-docs\//, /^docs\//,
];

// ...except these, which control the build, packaging, supply chain, automation or credentials.
const AGENT_DENIED = [
  /(^|\/)CMakeLists\.txt$/i, /\.cmake$/i, /^cmake\//i, /^scripts\//i, /^\.github\//i, /^relay\//i, /^tools\//i,
  /^update\//i, /^updater\//i, // the self-updater: it replaces the program on users' machines
  /^data\//i, /^brand\//i, /^LICENSE/i, /^THIRD-PARTY/i, /^CREDITS/i, /^SECURITY/i, /^CLAUDE\.md$/i,
  // anything executable or configuration-like, whatever its directory (Sphinx's conf.py is executed, for one)
  /\.(sh|bash|ps1|bat|cmd|py|pl|rb|js|mjs|cjs|yml|yaml|json|toml|lock|in|exe|dll|so|dylib|a|lib)$/i,
  /(^|\/)\./,
  // the reporting, branding and attribution code is security- and legally sensitive
  /ReportBugDialog|BugReport|LogBuffer|BrandTranslator|Branding|AboutDialog|(^|\/)Version\.|(^|\/)Config\./i,
];

export function agentMayChange(path) {
  return AGENT_ALLOWED.some((r) => r.test(path)) && !AGENT_DENIED.some((r) => r.test(path));
}

// Changes that ship to users.  Documentation, branding, automation and tooling do not need a release.
const NOT_SHIPPED = [/^docs\//, /^brand\//, /^\.github\//, /^tools\//, /^relay\//, /^user-docs\//, /\.md$/, /^LICENSE$/];

export function isShippable(path) {
  return !NOT_SHIPPED.some((r) => r.test(path));
}

// ------------------------------------------------------------------------------------------ risk

// Added lines matching these send a human to look: process execution, networking, dynamic loading,
// environment access, URLs, encoded blobs.  A bug fix rarely needs any of them.
// A long run of base64-like characters with real variety (ruler lines such as "-----" do not count).
function hasEncodedRun(line) {
  for (const m of line.matchAll(/[A-Za-z0-9+/=_-]{48,}/g)) {
    if (new Set(m[0]).size >= 14) return true;
  }
  return false;
}

// Tripwires are defence in depth, not the only barrier: the path allowlist, the council and CI also stand between
// an agent and a release.  They are matched per added line AND against the joined added text (with line
// continuations and adjacent string literals collapsed), so splitting a token across lines does not help.
export const TRIPWIRES = [
  [/\bQProcess\w*|\bstd::system\b|\bsystem\s*\(|\bpopen\s*\(|\b_?exec[lv]p?e?\s*\(|\bposix_spawn|\bfork\s*\(|\bvfork\s*\(|\bsyscall\s*\(|ShellExecute|WinExec|CreateProcess/i, 'process execution'],
  [/\bQNetwork\w*|\bQTcpSocket\b|\bQUdpSocket\b|\bQSslSocket\b|\bQWebSocket\b|\bQHostInfo\b|\bQHostAddress\b|\bsocket\s*\(|\bgetaddrinfo\b|\bgethostby\w*|sockaddr|WinHttp|InternetOpen|URLDownloadToFile|\bWSA\w*|<sys\/socket\.h>|<winsock/i, 'networking'],
  [/\bQDesktopServices\b|\bQLibrary\b|\bQPluginLoader\b|\bdlopen\b|\bdlsym\b|\bLoadLibrary|\bGetProcAddress\b/, 'launching or loading external code'],
  [/\bqgetenv\b|\bqEnvironmentVariable\w*|\bQProcessEnvironment\b|\bgetenv\b|\bsetenv\b|\bputenv\b|\benviron\b|\/proc\/|GetEnvironmentVariable/, 'environment access'],
  [/https?:\/\//i, 'URL'],
  [hasEncodedRun, 'long encoded string'],
  [/\\x[0-9a-fA-F]{2}(\\x[0-9a-fA-F]{2}){7,}/, 'escaped byte blob'],
  [/\beval\b|\bFunction\s*\(|##/, 'dynamic evaluation or token pasting'],
];

/** Collapse tricks that split a token over lines or adjacent string literals. */
function collapse(text) {
  return text.replace(/\\\r?\n/g, '').replace(/"\s*\n?\s*"/g, '');
}

// How the app looks and reads is a decision for a person.  Whatever the issue said, a change that edits a Qt Designer
// form or stylesheet, or adds or changes user-visible text, widgets, layout or styling, is held for human review.  (A
// crash fix almost never needs any of these; when it does, a person should look at it anyway.)
const UI_FILES = [/\.ui$/i, /\.qss$/i, /\.css$/i, /\.qrc$/i];
export const UI_SIGNALS = [
  [/\b(?:QObject::)?tr\s*\(|\bQT_TR(?:ANSLATE)?_NOOP\w*\s*\(|\bQCoreApplication::translate\s*\(/, 'user-visible text'],
  [/\bsetStyleSheet\s*\(|\bsetPalette\s*\(|\bsetFont\s*\(|\bsetIcon\w*\s*\(|\bQIcon\s*\(|\bsetStyle\s*\(/, 'styling'],
  [/\bsetWindowTitle\s*\(|\bsetToolTip\s*\(|\bsetStatusTip\s*\(|\bsetWhatsThis\s*\(|\bsetPlaceholderText\s*\(|\bsetText\s*\(\s*"/, 'user-visible text'],
  [/\b(?:add|insert)(?:Widget|Layout|Action|Menu|Tab|Item|Row|Spacing|Stretch)\s*\(|\bnew\s+Q(?:PushButton|ToolButton|Label|CheckBox|RadioButton|ComboBox|LineEdit|GroupBox|Dialog|Action|Menu|TabWidget|Splitter)\b|\bsetMinimumSize\s*\(|\bsetFixedSize\s*\(|\bsetContentsMargins\s*\(|\bsetSizePolicy\s*\(/, 'widgets or layout'],
];

// With the maintainer's `auto-fix` label (autofix.mjs) a SMALL change to what already exists is allowed: wording, spacing,
// sizes, colours, icons.  ADDING to the interface (widgets, actions, menus, layouts, new forms) stays a person's decision.
// Matched per added line AND against the joined, whitespace-flattened added text, so splitting a call over lines does not help.
// The XML tags are matched by NAME, whatever their attributes or the line breaks: Qt Designer does not care about either.
export const UI_STRUCTURAL = [
  /\b(?:add|insert)(?:Widget|Layout|Action|Actions|Menu|Tab|Item|Items|Row|Button|Separator|ToolBar|DockWidget|Page|Section|Column|Child)\s*\(/,
  /\bnew\b[^;]{0,80}\bQ[A-Za-z]*(?:Widget|Layout|Button|Box|Label|Frame|Dialog|Bar|Menu|Action|Edit|Slider|Splitter|View|List|Table|Tree|Tab|Stack|Area|Shortcut|Group|Dial|Browser|Window)\b/,
  /\bmake_(?:unique|shared)\s*<\s*Q[A-Za-z]*/,
  /\b(?:setLayout|setCentralWidget|setMenuBar|setStatusBar|setCornerWidget|setStandardButtons|setMenu|setParent|setBuddy|setTabOrder|connect)\s*\(/,
  /\bQMessageBox\s*::\s*\w+\s*\(/,
  /<(?:widget|action|addaction|layout|spacer|item|connection|connections|slot|signal|sender|receiver|customwidget|customwidgets|include|includes|header|resources|resource|tabstops|tabstop|buddy)\b/i,
];
// Macros and aliases can hide any of the above from a line-by-line check.
export const UI_INDIRECTION = /^\s*#\s*(?:define|undef)\b|\btypedef\b|\busing\s+\w+\s*=/;
// Up to the soft limit an auto-fix interface change is an ordinary low-risk change.  Beyond it (a good reason is needed)
// it is HIGH risk: every reviewer, Opus included, must agree and it merges only in the morning window.  Beyond the hard limit
// a person must decide.
export const UI_LIMITS = { softLines: 100, softFiles: 3, hardLines: 200, hardFiles: 6 };

// Areas where a regression hurts users most: file formats, printing, data merge, barcodes.
const HIGH_AREAS = [/^model\/Xml/, /^model\/PageRenderer/, /Zpl/i, /Print/i, /^merge\//, /^model\/Merge/, /^glbarcode\//, /^backends\//, /^glabels-fill\//];

// maxDiffChars must stay below what the council reviewers are shown (council-lib.mjs, 70000), so that nothing an
// automatic merge ships can fall outside what was reviewed.
export const LIMITS = { maxFiles: 12, maxChangedLines: 600, highFiles: 5, highChangedLines: 120, maxDiffChars: 60000 };

/**
 * Classify a change.
 *   files: [{ path, status: 'added'|'modified'|'removed'|'renamed', additions, deletions, binary?, mode? }]
 *   addedLines: array of added line texts (without the leading "+")
 *   options.diffChars: size of the whole diff text, when known
 *   options.autoFix: the issue carries an approved `auto-fix` label, so a small change to the existing interface is allowed
 * Returns { risk: 'low' | 'high' | 'blocked', reasons: string[], ui: boolean } (ui: the change touches the interface).
 */
export function classifyChange(files, addedLines = [], options = {}) {
  const blocked = [];
  const high = [];
  const autoFix = options.autoFix === true;
  let ui = false;

  if (files.length === 0) blocked.push('empty change');
  if ((options.diffChars ?? 0) > LIMITS.maxDiffChars) blocked.push(`diff is ${options.diffChars} characters (max ${LIMITS.maxDiffChars})`);
  if (files.length > LIMITS.maxFiles) blocked.push(`touches ${files.length} files (max ${LIMITS.maxFiles})`);

  let changed = 0;
  for (const f of files) {
    changed += (f.additions ?? 0) + (f.deletions ?? 0);
    if (!agentMayChange(f.path)) blocked.push(`out-of-scope path: ${f.path}`);
    if (f.binary) blocked.push(`binary file: ${f.path}`);
    if (f.mode && f.mode !== '100644') blocked.push(`file mode ${f.mode}: ${f.path}`);
    const isTest = /(^|\/)(unit_tests|tests?)\//i.test(f.path);
    if (f.status === 'removed' && isTest) blocked.push(`removes a test: ${f.path}`);
    if (isTest && f.status !== 'added' && (f.deletions ?? 0) > (f.additions ?? 0) + 5) blocked.push(`weakens a test: ${f.path}`);
    if (f.status === 'removed' || f.status === 'renamed') high.push(`${f.status} file: ${f.path}`);
    if (HIGH_AREAS.some((r) => r.test(f.path))) high.push(`sensitive area: ${f.path}`);
  }
  if (changed > LIMITS.maxChangedLines) blocked.push(`${changed} changed lines (max ${LIMITS.maxChangedLines})`);
  if (changed > LIMITS.highChangedLines) high.push(`${changed} changed lines`);
  if (files.length > LIMITS.highFiles) high.push(`${files.length} files`);

  for (const f of files) {
    if (!UI_FILES.some((r) => r.test(f.path))) continue;
    ui = true;
    if (!autoFix) blocked.push(`user-interface change (needs human review): ${f.path}`);
    else if (/\.qrc$/i.test(f.path)) blocked.push(`resource manifest (a person decides): ${f.path}`);
    else if (f.status !== 'modified') blocked.push(`new, removed or renamed interface file (a person decides): ${f.path}`);
  }
  const uiSeen = new Set();
  for (const line of addedLines) {
    if (autoFix && UI_STRUCTURAL.some((r) => r.test(line))) {
      ui = true;
      blocked.push(`adds to the interface - a widget, action, menu or layout (a person decides): ${line.trim().slice(0, 70)}`);
      continue;
    }
    for (const [test, label] of UI_SIGNALS) {
      if (!test.test(line)) continue;
      ui = true;
      if (!autoFix && !uiSeen.has(label)) {
        uiSeen.add(label);
        blocked.push(`user-interface change (needs human review): added line changes ${label}: ${line.trim().slice(0, 70)}`);
      }
    }
  }
  if (autoFix) {
    // The label is for SMALL changes, whatever they touch: the size limits apply to every auto-fix change.
    if (changed > UI_LIMITS.hardLines || files.length > UI_LIMITS.hardFiles) {
      blocked.push(`auto-fix change of ${changed} lines in ${files.length} files (auto-fix allows at most ${UI_LIMITS.hardLines} lines in ${UI_LIMITS.hardFiles} files)`);
    } else if (changed > UI_LIMITS.softLines || files.length > UI_LIMITS.softFiles) {
      high.push(`auto-fix change of ${changed} lines in ${files.length} files (small is ${UI_LIMITS.softLines} lines in ${UI_LIMITS.softFiles} files)`);
    }
    const flat = collapse(addedLines.join('\n')).replace(/\s+/g, ' ');
    if (UI_STRUCTURAL.some((r) => r.test(flat))) {
      ui = true;
      blocked.push('adds to the interface - a widget, action, menu, layout, connection or include (a person decides), seen after joining lines');
    }
    for (const line of addedLines) {
      if (UI_INDIRECTION.test(line)) { blocked.push(`macro or alias that could hide a change (a person decides): ${line.trim().slice(0, 70)}`); break; }
    }
  }

  const trips = (test, text) => (typeof test === 'function' ? test(text) : test.test(text));
  for (const line of addedLines) {
    for (const [test, label] of TRIPWIRES) {
      if (trips(test, line)) {
        blocked.push(`added line trips "${label}": ${line.trim().slice(0, 80)}`);
        break;
      }
    }
  }
  const joined = collapse(addedLines.join('\n'));
  for (const [test, label] of TRIPWIRES) {
    if (trips(test, joined)) blocked.push(`added code trips "${label}" (after joining lines)`);
  }

  if (blocked.length) return { risk: 'blocked', reasons: [...new Set(blocked)], ui };
  if (high.length) return { risk: 'high', reasons: [...new Set(high)], ui };
  return { risk: 'low', reasons: [], ui };
}

const ORDER = { low: 0, high: 1, blocked: 2 };

/** The most severe of several risk labels. */
export function maxRisk(...risks) {
  return risks.reduce((a, b) => (ORDER[b] > ORDER[a] ? b : a), 'low');
}

/** Added lines from a unified diff (hunk-aware: a content line that starts with "++" is still content). */
export function addedLinesOf(diffText) {
  return parseDiff(diffText).addedLines;
}

// ------------------------------------------------------------------------------------------ time

/** Local date/time parts in Chicago. */
export function chicago(now = new Date()) {
  const parts = new Intl.DateTimeFormat('en-CA', {
    timeZone: TIME_ZONE, year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hourCycle: 'h23',
  }).formatToParts(now);
  const get = (t) => parts.find((p) => p.type === t).value;
  return { date: `${get('year')}-${get('month')}-${get('day')}`, hour: Number(get('hour')), minute: Number(get('minute')) };
}

// Risky merges are only auto-merged in this early-morning window (Chicago time), so that they ship in the
// 10:00 release with the whole day to watch them; the 14:00 release carries only the day's low-risk fixes.
export const HIGH_RISK_WINDOW = { startHour: 5, endHour: 9 };

/**
 * May an approved PR be merged right now?  Returns { ok, reason }.
 */
export function mayAutoMerge(risk, level, now = new Date()) {
  if (risk === 'blocked') return { ok: false, reason: 'blocked changes always need a human' };
  if (risk === 'low') {
    return level >= 3 ? { ok: true, reason: 'low risk, automation level >= 3' } : { ok: false, reason: 'automation level < 3' };
  }
  if (level < 4) return { ok: false, reason: 'high risk needs automation level 4' };
  const { hour } = chicago(now);
  if (hour >= HIGH_RISK_WINDOW.startHour && hour < HIGH_RISK_WINDOW.endHour) return { ok: true, reason: 'high risk inside the morning window' };
  return { ok: false, reason: `high risk merges only ${HIGH_RISK_WINDOW.startHour}:00-${HIGH_RISK_WINDOW.endHour}:00 Chicago time` };
}

// ------------------------------------------------------------------------------------------ releases

// GitHub's scheduler can start a run well after its cron time, so each slot is a window rather than an instant.
// A slot can only be used once (counted from the release runs the train itself started), and at most two a day.
export const RELEASE_WINDOWS = [{ slot: 10, from: 10 * 60, to: 11 * 60 + 30 }, { slot: 14, from: 14 * 60, to: 15 * 60 + 30 }];
export const MAX_RELEASES_PER_DAY = 2;

/** Which release slot (10 or 14) a Chicago time falls into, or null. */
export function slotAt(t) {
  const minutes = t.hour * 60 + t.minute;
  return RELEASE_WINDOWS.find((w) => minutes >= w.from && minutes < w.to)?.slot ?? null;
}

/**
 * Decide whether the release train should publish now.
 *   now              Date
 *   trainRuns        [{ created_at }]  release workflow runs on version tags (any outcome) - these are the slots used
 *   shippableChanged boolean           anything user-visible landed since the last stable tag
 *   headBuilt        boolean           the code on main has a successful build
 *   ignoreClock      boolean           maintainer override: skip the slot window (the daily cap still applies)
 * Returns { release: boolean, reason: string, slot?: number | 'manual' }.
 */
export function releaseDecision({ now = new Date(), trainRuns, shippableChanged, headBuilt, ignoreClock = false }) {
  const t = chicago(now);
  const slot = slotAt(t);
  if (slot === null && !ignoreClock) {
    return { release: false, reason: `not inside a release window (Chicago ${String(t.hour).padStart(2, '0')}:${String(t.minute).padStart(2, '0')})` };
  }

  const today = trainRuns.filter((r) => chicago(new Date(r.created_at)).date === t.date);
  if (today.length >= MAX_RELEASES_PER_DAY) return { release: false, reason: `already ${today.length} releases today`, slot: slot ?? 'manual' };
  if (slot !== null && today.some((r) => slotAt(chicago(new Date(r.created_at))) === slot)) {
    return { release: false, reason: `the ${slot}:00 slot was already used`, slot };
  }
  if (!shippableChanged) return { release: false, reason: 'no user-visible changes since the last release', slot: slot ?? 'manual' };
  if (!headBuilt) return { release: false, reason: 'the code on main has no successful build yet', slot: slot ?? 'manual' };
  return { release: true, reason: slot === null ? 'maintainer override' : `release slot ${slot}:00`, slot: slot ?? 'manual' };
}

export const MAX_RELEASE_ATTEMPTS = 3;

/**
 * A release build that died on infrastructure (for example no macOS runner was free for 15 minutes) must not burn its
 * slot.  Returns the train's release run for the slot that is open right now if it ended in failure or cancellation and
 * has been attempted fewer than MAX_RELEASE_ATTEMPTS times, so that the train can re-run its failed jobs; otherwise null.
 *   trainRuns  [{ id, created_at, conclusion, status, run_attempt }]  release runs on version tags started by the train
 */
export function retryableRun({ now = new Date(), trainRuns }) {
  const t = chicago(now);
  const slot = slotAt(t);
  if (slot === null) return null;
  const mine = trainRuns
    .filter((r) => chicago(new Date(r.created_at)).date === t.date && slotAt(chicago(new Date(r.created_at))) === slot)
    .sort((a, b) => new Date(b.created_at) - new Date(a.created_at));
  const run = mine[0];
  if (!run || run.status !== 'completed' || !['failure', 'cancelled', 'timed_out'].includes(run.conclusion)) return null;
  if ((run.run_attempt ?? 1) >= MAX_RELEASE_ATTEMPTS) return null;
  return run;
}

/** Next version: minor for features, patch otherwise.  Major bumps are manual. */
export function nextVersion(lastTag, subjects) {
  const m = /^v(\d+)\.(\d+)\.(\d+)$/.exec(lastTag ?? '');
  if (!m) throw new Error(`cannot parse tag: ${lastTag}`);
  let [major, minor, patch] = m.slice(1).map(Number);
  const feature = subjects.some((s) => /^feat(\([^)]*\))?!?:/.test(s) || /^[a-z]+(\([^)]*\))?!:/.test(s));
  if (feature) {
    minor += 1;
    patch = 0;
  } else {
    patch += 1;
  }
  return `v${major}.${minor}.${patch}`;
}
