// Pure policy functions for the automation pipeline.  No I/O, so they can be unit-tested exhaustively.
//
// The rules here are the safety boundary between "an agent wrote this" and "this reaches users":
//   - which paths an agent may change at all,
//   - which patterns force a human to look,
//   - when (Chicago time) risky changes may be merged,
//   - when a release may be cut.

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
  /(^|\/)CMakeLists\.txt$/, /\.cmake$/, /^cmake\//, /^scripts\//, /^\.github\//, /^relay\//, /^tools\//,
  /^data\//, /^brand\//, /^LICENSE/, /^THIRD-PARTY/, /^CREDITS/, /^SECURITY/, /^CLAUDE\.md$/,
  /\.(sh|ps1|bat|cmd|yml|yaml|json|toml|lock|in)$/, /(^|\/)\./,
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

export const TRIPWIRES = [
  [/\bQProcess\b|\bstd::system\b|\bsystem\s*\(|\bpopen\s*\(|\bexec[lv]p?e?\s*\(|ShellExecute|WinExec|CreateProcess/, 'process execution'],
  [/\bQNetwork\w*|\bQTcpSocket\b|\bQUdpSocket\b|\bQSslSocket\b|\bQWebSocket\b|\bsocket\s*\(|\bconnect\s*\(\s*\w+\s*,\s*\(?\s*struct sockaddr/, 'networking'],
  [/\bQDesktopServices\b|\bQLibrary\b|\bdlopen\b|\bLoadLibrary/, 'launching or loading external code'],
  [/\bqgetenv\b|\bqEnvironmentVariable\b|\bgetenv\b|\bsetenv\b|\bputenv\b/, 'environment access'],
  [/https?:\/\//, 'URL'],
  [hasEncodedRun, 'long encoded string'],
  [/\\x[0-9a-fA-F]{2}(\\x[0-9a-fA-F]{2}){7,}/, 'escaped byte blob'],
  [/\beval\b|\bFunction\s*\(/, 'dynamic evaluation'],
];

// Areas where a regression hurts users most: file formats, printing, data merge, barcodes.
const HIGH_AREAS = [/^model\/Xml/, /^model\/PageRenderer/, /Zpl/i, /Print/i, /^merge\//, /^model\/Merge/, /^glbarcode\//, /^backends\//, /^glabels-fill\//];

export const LIMITS = { maxFiles: 12, maxChangedLines: 600, highFiles: 5, highChangedLines: 120 };

/**
 * Classify a change.
 *   files: [{ path, status: 'added'|'modified'|'removed'|'renamed', additions, deletions, binary?, mode? }]
 *   addedLines: array of added line texts (without the leading "+")
 * Returns { risk: 'low' | 'high' | 'blocked', reasons: string[] }.
 */
export function classifyChange(files, addedLines = []) {
  const blocked = [];
  const high = [];

  if (files.length === 0) blocked.push('empty change');
  if (files.length > LIMITS.maxFiles) blocked.push(`touches ${files.length} files (max ${LIMITS.maxFiles})`);

  let changed = 0;
  for (const f of files) {
    changed += (f.additions ?? 0) + (f.deletions ?? 0);
    if (!agentMayChange(f.path)) blocked.push(`out-of-scope path: ${f.path}`);
    if (f.binary) blocked.push(`binary file: ${f.path}`);
    if (f.mode && f.mode !== '100644') blocked.push(`file mode ${f.mode}: ${f.path}`);
    if (f.status === 'removed' && /(^|\/)(unit_tests|tests?)\//.test(f.path)) blocked.push(`removes a test: ${f.path}`);
    if (f.status === 'removed' || f.status === 'renamed') high.push(`${f.status} file: ${f.path}`);
    if (HIGH_AREAS.some((r) => r.test(f.path))) high.push(`sensitive area: ${f.path}`);
  }
  if (changed > LIMITS.maxChangedLines) blocked.push(`${changed} changed lines (max ${LIMITS.maxChangedLines})`);
  if (changed > LIMITS.highChangedLines) high.push(`${changed} changed lines`);
  if (files.length > LIMITS.highFiles) high.push(`${files.length} files`);

  for (const line of addedLines) {
    for (const [test, label] of TRIPWIRES) {
      if (typeof test === 'function' ? test(line) : test.test(line)) {
        blocked.push(`added line trips "${label}": ${line.trim().slice(0, 80)}`);
        break;
      }
    }
  }

  if (blocked.length) return { risk: 'blocked', reasons: [...new Set(blocked)] };
  if (high.length) return { risk: 'high', reasons: [...new Set(high)] };
  return { risk: 'low', reasons: [] };
}

const ORDER = { low: 0, high: 1, blocked: 2 };

/** The most severe of several risk labels. */
export function maxRisk(...risks) {
  return risks.reduce((a, b) => (ORDER[b] > ORDER[a] ? b : a), 'low');
}

/** Added lines from a unified diff. */
export function addedLinesOf(diffText) {
  return diffText
    .split('\n')
    .filter((l) => l.startsWith('+') && !l.startsWith('+++'))
    .map((l) => l.slice(1));
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

export const RELEASE_HOURS = [10, 14];
export const MAX_RELEASES_PER_DAY = 2;

/**
 * Decide whether the release train should publish now.
 *   now              Date
 *   releases         [{ tag_name, published_at, draft, prerelease }]  (stable and edge; edge is ignored)
 *   shippableChanged boolean   anything user-visible landed since the last stable tag
 *   headBuilt        boolean   the newest commit on main has a successful edge build
 * Returns { release: boolean, reason: string, slot?: number }.
 */
export function releaseDecision({ now = new Date(), releases, shippableChanged, headBuilt }) {
  const t = chicago(now);
  if (!RELEASE_HOURS.includes(t.hour)) return { release: false, reason: `not a release hour (Chicago ${t.hour}:00)` };
  const slot = t.hour;

  const today = releases.filter(
    (r) => !r.draft && !r.prerelease && /^v\d+\.\d+\.\d+$/.test(r.tag_name) && chicago(new Date(r.published_at)).date === t.date,
  );
  if (today.length >= MAX_RELEASES_PER_DAY) return { release: false, reason: `already ${today.length} releases today`, slot };
  if (today.some((r) => chicago(new Date(r.published_at)).hour >= slot)) {
    return { release: false, reason: `the ${slot}:00 slot already released`, slot };
  }
  if (!shippableChanged) return { release: false, reason: 'no user-visible changes since the last release', slot };
  if (!headBuilt) return { release: false, reason: 'the newest commit has no successful build yet', slot };
  return { release: true, reason: `release slot ${slot}:00`, slot };
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
