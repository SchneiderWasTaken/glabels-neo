// Stage 2: let a model attempt a fix for one issue.
//
// Containment:
//   * The model can only Read/Glob/Grep/Edit/Write files in the checkout.  It has no shell and no network
//     tool, and reads outside the checkout are denied, so it cannot reach this process's environment.
//   * Building and testing are done by THIS script with the credentials removed from the environment, so no
//     code the model wrote ever runs next to a secret.
//   * The resulting diff is classified (policy.mjs) and scanned for secrets before it leaves this job.
//   * Nothing is pushed here.  fix-publish.mjs (no model credential) opens the draft PR.

import { spawnSync } from 'node:child_process';
import { writeFileSync } from 'node:fs';

import {
  REPO, UNTRUSTED_NOTICE, api, assertNoSecrets, runClaude, sanitizeForComment, secretsFromEnv, untrusted, writeJson,
} from './lib.mjs';
import { addedLinesOf, classifyChange } from './policy.mjs';

const number = Number(process.env.ISSUE_NUMBER);
if (!Number.isInteger(number) || number <= 0) throw new Error('ISSUE_NUMBER is required');

const MAX_ATTEMPTS = Number(process.env.FIX_MAX_ATTEMPTS || 3);
const MODEL = process.env.FIX_MODEL || 'sonnet';
const SECRETS = secretsFromEnv('CLAUDE_CODE_OAUTH_TOKEN', 'GH_TOKEN', 'GITHUB_TOKEN');

const FIX_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['cannot_fix', 'summary', 'tests_added'],
  properties: {
    cannot_fix: { type: 'boolean' },
    reason: { type: 'string', maxLength: 400 },
    summary: { type: 'string', maxLength: 700 },
    tests_added: { type: 'boolean' },
  },
};

const issue = api(`repos/${REPO}/issues/${number}`);
if (issue.pull_request) throw new Error('not an issue');

/** Environment for build/test subprocesses: everything except credentials. */
function cleanEnv() {
  const env = { ...process.env };
  for (const k of Object.keys(env)) {
    if (/TOKEN|SECRET|PASSWORD|CREDENTIAL|^ANTHROPIC_|^CLAUDE_/i.test(k)) delete env[k];
  }
  return env;
}

function sh(cmd, args, opts = {}) {
  return spawnSync(cmd, args, { encoding: 'utf8', maxBuffer: 128 * 1024 * 1024, ...opts });
}

const CPUS = Number.parseInt(sh('nproc', []).stdout, 10) || 2;

function buildAndTest() {
  const env = cleanEnv();
  const build = sh('cmake', ['--build', 'build', '-j', String(CPUS)], { env });
  if (build.status !== 0) return { ok: false, log: `BUILD FAILED\n${tail(build.stdout + build.stderr, 80)}` };
  const test = sh('ctest', ['--test-dir', 'build', '--output-on-failure', '--timeout', '120'], { env });
  if (test.status !== 0) return { ok: false, log: `TESTS FAILED\n${tail(test.stdout + test.stderr, 80)}` };
  return { ok: true, log: tail(test.stdout, 15) };
}

function tail(text, lines) {
  return String(text).split('\n').slice(-lines).join('\n');
}

/** Stage everything and describe the change. */
function currentChange() {
  sh('git', ['add', '-A']);
  const nameStatus = sh('git', ['diff', '--cached', '--name-status', '--no-renames']).stdout.trim().split('\n').filter(Boolean);
  const numstat = sh('git', ['diff', '--cached', '--numstat', '--no-renames']).stdout.trim().split('\n').filter(Boolean);
  // Paths whose file mode is not plain 100644 (new executables, mode changes, symlinks).
  const oddModes = new Map();
  for (const line of sh('git', ['diff', '--cached', '--summary']).stdout.split('\n')) {
    const m = /^ (?:create mode (\d+)|mode change \d+ => (\d+)) (.+)$/.exec(line);
    if (m) oddModes.set(m[3], m[1] ?? m[2]);
  }
  const patch = sh('git', ['diff', '--cached', '--no-color', '--no-renames']).stdout;

  const files = numstat.map((line, i) => {
    const [add, del, ...rest] = line.split('\t');
    const path = rest.join('\t').replace(/^.*=> /, '').replace(/[{}]/g, '');
    const code = (nameStatus[i] ?? 'M').charAt(0);
    return {
      path,
      status: { A: 'added', D: 'removed', R: 'renamed' }[code] ?? 'modified',
      additions: add === '-' ? 0 : Number(add),
      deletions: del === '-' ? 0 : Number(del),
      binary: add === '-',
      mode: oddModes.get(path),
    };
  });
  return { files, patch, classification: classifyChange(files, addedLinesOf(patch)) };
}

const repoGuidance = `Project: gLabels Neo, a Qt6 / C++20 label designer (CMake). Code style: see docs/CODING-STYLE.md
(8-space indent, braces on their own lines, spaces inside parentheses). Unit tests live in model/unit_tests
(Qt Test; see TestStrUtil.cpp for the pattern) - add or extend a test for the behaviour you fix whenever the code
is in model/ or is otherwise testable.`;

const rules = `Rules:
- Make the smallest change that fixes the problem. Do not refactor, rename, reformat or "improve" other code.
- You may only change files under glabels/, model/, glbarcode/, glabels-batch/, glabels-fill/, backends/,
  templates/, translations/, test-data/, user-docs/ or docs/. NEVER change CMakeLists.txt, *.cmake, scripts,
  .github, packaging, licenses, or any dotfile.
- Do not add network access, process execution, dynamic library loading, environment-variable reads or URLs.
- Do not delete or weaken tests. Do not add binary files.
- If the report is unclear, describes something that is not a bug, or you cannot fix it safely, set cannot_fix
  true and explain briefly. That is a good outcome.
- You cannot run the build; it is run for you after you finish, and failures are reported back to you.`;

let feedback = '';
let last = null;
let outcome = null;

for (let attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
  const prompt = [
    UNTRUSTED_NOTICE,
    '',
    repoGuidance,
    '',
    rules,
    '',
    'Fix this issue.',
    untrusted('issue-title', issue.title, 300),
    untrusted('issue-body', issue.body, 12000),
    feedback ? `\nYour previous attempt had problems:\n${untrusted('feedback', feedback, 6000)}\nFix them.` : '',
    '',
    'When finished, answer with the JSON object requested (summary: what you changed and why, in plain words).',
  ].join('\n');

  console.log(`attempt ${attempt}/${MAX_ATTEMPTS} with ${MODEL}`);
  const result = await runClaude({
    prompt,
    model: MODEL,
    tools: 'Read,Glob,Grep,Edit,Write',
    permissionMode: 'acceptEdits',
    schema: FIX_SCHEMA,
    maxBudgetUsd: Number(process.env.FIX_BUDGET_USD || 3),
    timeoutMs: 25 * 60 * 1000,
    system: 'You are a careful C++ maintainer fixing one defect. You can only read and edit files.',
  });
  last = result.structured ?? { cannot_fix: true, reason: 'no structured answer', summary: '', tests_added: false };

  if (last.cannot_fix) {
    outcome = { status: 'cannot-fix', reason: sanitizeForComment(last.reason ?? '', 400) };
    break;
  }

  const change = currentChange();
  console.log(`change: ${change.files.length} files, risk ${change.classification.risk}`);
  if (change.files.length === 0) {
    feedback = 'You made no changes.';
    continue;
  }
  if (change.classification.risk === 'blocked') {
    outcome = { status: 'blocked', reasons: change.classification.reasons.slice(0, 10).map((r) => sanitizeForComment(r, 200)) };
    break;
  }

  const built = buildAndTest();
  console.log(`build/test: ${built.ok ? 'ok' : 'failed'}`);
  if (!built.ok) {
    feedback = built.log;
    continue;
  }

  outcome = {
    status: 'ok',
    summary: sanitizeForComment(last.summary, 700),
    testsAdded: Boolean(last.tests_added),
    classification: change.classification,
    files: change.files.map((f) => f.path),
    attempts: attempt,
  };
  assertNoSecrets(change.patch, SECRETS);
  writeFileSync('fix.patch', change.patch);
  break;
}

if (!outcome) outcome = { status: 'failed', reason: sanitizeForComment(feedback, 600) };
const out = { number, title: issue.title, outcome };
assertNoSecrets(JSON.stringify(out), SECRETS);
writeJson('fix-result.json', out);
console.log(`result: ${outcome.status}`);
