// Stage 2: let a model attempt a fix for one issue, in rounds.  One round is three workflow steps:
//
//   prepare  (no model credential)  snapshot the working tree into ./agent-work (a plain directory, NO .git),
//                                   fetch the issue
//   agent    (model credential)     the model edits files in ./agent-work.  It has no shell and no network tool, reads
//                                   outside the directory are denied, and there is no .git directory whose config it
//                                   could poison.  Nothing else runs in this step.
//   verify   (no model credential)  copy the edited files over the real checkout, classify the diff against the
//                                   policy, and only if it is not "blocked" build and run the tests
//
// The step that holds the credential never builds or runs anything the model wrote, and the steps that build and run
// it never hold the credential - and they are not descendants of the step that does, so they cannot read its
// environment either.  Nothing is pushed here; fix-publish.mjs (no model credential) opens the draft PR.

import { spawnSync } from 'node:child_process';
import {
  copyFileSync, existsSync, lstatSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync,
} from 'node:fs';
import { createHash } from 'node:crypto';
import { dirname, join, relative } from 'node:path';

import {
  REPO, UNTRUSTED_NOTICE, api, assertNoSecrets, readJson, runClaude, sanitizeForComment, secretsFromEnv, untrusted, writeJson,
} from './lib.mjs';
import { addedLinesOf, classifyChange } from './policy.mjs';

const mode = process.argv[2];
const WORK = 'agent-work';
const STATE = 'fix-state.json';
const MAX_ROUNDS = Number(process.env.FIX_MAX_ROUNDS || 3);

const TERMINAL = new Set(['ok', 'cannot-fix', 'blocked', 'failed']);
const state = existsSync(STATE) ? readJson(STATE) : { round: 0, status: 'continue', feedback: '' };
const finished = () => TERMINAL.has(state.status);

function sh(cmd, args, opts = {}) {
  return spawnSync(cmd, args, { encoding: 'utf8', maxBuffer: 256 * 1024 * 1024, ...opts });
}
const tail = (text, lines) => String(text).split('\n').slice(-lines).join('\n');
const sha256 = (path) => createHash('sha256').update(readFileSync(path)).digest('hex');

/** All regular files under dir (relative paths); throws on symlinks and other oddities. */
function listFiles(dir) {
  const out = [];
  (function walk(d) {
    for (const entry of readdirSync(d, { withFileTypes: true })) {
      const full = join(d, entry.name);
      const st = lstatSync(full);
      if (st.isSymbolicLink()) throw new Error(`symlink in agent output: ${relative(dir, full)}`);
      if (st.isDirectory()) walk(full);
      else if (st.isFile()) out.push(relative(dir, full).split('\\').join('/'));
      else throw new Error(`unexpected file type: ${relative(dir, full)}`);
    }
  })(dir);
  return out;
}

// ----------------------------------------------------------------------------------------------- prepare
function prepare() {
  if (finished()) return;
  state.round += 1;

  // Keep our own bookkeeping files out of "git add -A" (local exclude file; the agent never sees .git).
  const exclude = '.git/info/exclude';
  const have = existsSync(exclude) ? readFileSync(exclude, 'utf8') : '';
  if (!have.includes('# fix-step')) {
    mkdirSync(dirname(exclude), { recursive: true });
    const patterns = ['# fix-step', '/agent-work/', '/fix-*.json', '/agent-answer.json', '/fix.patch', '/commit-message.txt', '/pr-body.md'];
    writeFileSync(exclude, `${have}\n${patterns.join('\n')}\n`);
  }

  if (!existsSync('fix-issue.json')) {
    const number = Number(process.env.ISSUE_NUMBER);
    if (!Number.isInteger(number) || number <= 0) throw new Error('ISSUE_NUMBER is required');
    const issue = api(`repos/${REPO}/issues/${number}`);
    if (issue.pull_request) throw new Error('not an issue');
    writeJson('fix-issue.json', { number, title: issue.title, body: issue.body ?? '' });
  }

  rmSync(WORK, { recursive: true, force: true });
  mkdirSync(WORK);
  // Tracked and untracked (non-ignored) files of the current working tree, without .git.
  const list = sh('git', ['ls-files', '-z', '--cached', '--others', '--exclude-standard']);
  if (list.status !== 0) throw new Error('git ls-files failed');
  const paths = list.stdout.split('\0').filter(Boolean);
  const manifest = {};
  for (const p of paths) {
    const src = p;
    if (!existsSync(src) || lstatSync(src).isSymbolicLink() || !lstatSync(src).isFile()) continue;
    mkdirSync(dirname(join(WORK, p)), { recursive: true });
    copyFileSync(src, join(WORK, p));
    manifest[p] = sha256(src);
  }
  writeJson('fix-manifest.json', manifest);
  writeJson(STATE, state);
  console.log(`round ${state.round}: snapshot of ${paths.length} files`);
}

// -------------------------------------------------------------------------------------------------- agent
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

async function agent() {
  if (finished()) return;
  const issue = readJson('fix-issue.json');
  const model = process.env.FIX_MODEL || 'sonnet';
  const prompt = [
    UNTRUSTED_NOTICE, '', repoGuidance, '', rules, '', 'Fix this issue.',
    untrusted('issue-title', issue.title, 300),
    untrusted('issue-body', issue.body, 12000),
    state.feedback ? `\nYour previous attempt had problems:\n${untrusted('feedback', state.feedback, 6000)}\nFix them.` : '',
    '', 'When finished, answer with the JSON object requested (summary: what you changed and why, in plain words).',
  ].join('\n');

  console.log(`round ${state.round}/${MAX_ROUNDS}: asking ${model}`);
  const result = await runClaude({
    prompt, model, tools: 'Read,Glob,Grep,Edit,Write', permissionMode: 'acceptEdits', schema: FIX_SCHEMA,
    cwd: WORK, maxBudgetUsd: Number(process.env.FIX_BUDGET_USD || 3), timeoutMs: 25 * 60 * 1000,
    system: 'You are a careful C++ maintainer fixing one defect. You can only read and edit files.',
  });
  const answer = result.structured ?? { cannot_fix: true, reason: 'no structured answer', summary: '', tests_added: false };

  // Scan everything that changed (and the answer) for credentials, while we still hold them.
  const secrets = secretsFromEnv('CLAUDE_CODE_OAUTH_TOKEN');
  const manifest = readJson('fix-manifest.json');
  for (const f of listFiles(WORK)) {
    const changed = manifest[f] === undefined || sha256(join(WORK, f)) !== manifest[f];
    if (changed) assertNoSecrets(readFileSync(join(WORK, f), 'latin1'), secrets);
  }
  assertNoSecrets(JSON.stringify(answer), secrets);
  writeJson('agent-answer.json', answer);
}

// ------------------------------------------------------------------------------------------------- verify
function cleanEnv() {
  const env = { ...process.env };
  for (const k of Object.keys(env)) if (/TOKEN|SECRET|PASSWORD|CREDENTIAL|^ANTHROPIC_|^CLAUDE_/i.test(k)) delete env[k];
  return env;
}

function currentChange() {
  sh('git', ['add', '-A']);
  const nameStatus = sh('git', ['diff', '--cached', '--name-status', '--no-renames']).stdout.trim().split('\n').filter(Boolean);
  const numstat = sh('git', ['diff', '--cached', '--numstat', '--no-renames']).stdout.trim().split('\n').filter(Boolean);
  const oddModes = new Map();
  for (const line of sh('git', ['diff', '--cached', '--summary']).stdout.split('\n')) {
    const m = /^ (?:create mode (\d+)|mode change \d+ => (\d+)) (.+)$/.exec(line);
    if (m) oddModes.set(m[3], m[1] ?? m[2]);
  }
  const patch = sh('git', ['diff', '--cached', '--no-color', '--no-renames']).stdout;
  const files = numstat.map((line, i) => {
    const [add, del, ...rest] = line.split('\t');
    const path = rest.join('\t');
    const code = (nameStatus[i] ?? 'M').charAt(0);
    return {
      path,
      status: { A: 'added', D: 'removed' }[code] ?? 'modified',
      additions: add === '-' ? 0 : Number(add),
      deletions: del === '-' ? 0 : Number(del),
      binary: add === '-',
      mode: oddModes.get(path),
    };
  });
  return { files, patch, classification: classifyChange(files, addedLinesOf(patch)) };
}

function buildAndTest() {
  const env = cleanEnv();
  const cpus = Number.parseInt(sh('nproc', []).stdout, 10) || 2;
  const build = sh('cmake', ['--build', 'build', '-j', String(cpus)], { env });
  if (build.status !== 0) return { ok: false, log: `BUILD FAILED\n${tail(build.stdout + build.stderr, 80)}` };
  const test = sh('ctest', ['--test-dir', 'build', '--output-on-failure', '--timeout', '120'], { env });
  if (test.status !== 0) return { ok: false, log: `TESTS FAILED\n${tail(test.stdout + test.stderr, 80)}` };
  return { ok: true, log: tail(test.stdout, 15) };
}

function finish(outcome) {
  state.status = outcome.status;
  writeJson(STATE, state);
  writeJson('fix-result.json', { number: readJson('fix-issue.json').number, title: readJson('fix-issue.json').title, outcome });
  console.log(`result: ${outcome.status}`);
}

function verify() {
  if (finished()) return;
  const answer = readJson('agent-answer.json');

  if (answer.cannot_fix) {
    return finish({ status: 'cannot-fix', reason: sanitizeForComment(answer.reason ?? '', 400) });
  }

  // Overlay the agent's files onto the real checkout (regular files only; it cannot delete anything).
  const manifest = readJson('fix-manifest.json');
  for (const f of listFiles(WORK)) {
    if (/(^|\/)\.git(\/|$)/.test(f) || f.includes('..')) continue;
    const src = join(WORK, f);
    if (manifest[f] !== undefined && sha256(src) === manifest[f]) continue;
    mkdirSync(dirname(f) || '.', { recursive: true });
    copyFileSync(src, f);
  }

  const change = currentChange();
  console.log(`change: ${change.files.length} files, risk ${change.classification.risk}`);
  if (change.files.length === 0) {
    state.feedback = 'You made no changes.';
  } else if (change.classification.risk === 'blocked') {
    return finish({ status: 'blocked', reasons: change.classification.reasons.slice(0, 10).map((r) => sanitizeForComment(r, 200)) });
  } else {
    const built = buildAndTest();
    console.log(`build/test: ${built.ok ? 'ok' : 'failed'}`);
    if (built.ok) {
      writeFileSync('fix.patch', change.patch);
      return finish({
        status: 'ok',
        summary: sanitizeForComment(answer.summary, 700),
        testsAdded: Boolean(answer.tests_added),
        classification: change.classification,
        files: change.files.map((f) => f.path),
        attempts: state.round,
      });
    }
    state.feedback = built.log;
  }

  if (state.round >= MAX_ROUNDS) {
    return finish({ status: 'failed', reason: sanitizeForComment(state.feedback, 600) });
  }
  writeJson(STATE, state);
}

if (mode === 'prepare') prepare();
else if (mode === 'agent') await agent();
else if (mode === 'verify') verify();
else throw new Error(`unknown mode: ${mode}`);
