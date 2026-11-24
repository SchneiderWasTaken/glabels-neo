// Stage 2: let a model attempt a fix for one issue.  A round is two JOBS, on separate machines:
//
//   agent job   (holds the model credential)  prepare + agent
//       prepare  copy the checkout into ./agent-work (a plain directory, NO .git), fetch the issue and check that it
//                is still the text that was approved, overlay the previous round's edits
//       agent    the model edits files in ./agent-work.  It has no shell and no network tool; reads outside the
//                directory are denied; there is no git configuration to poison.  The changed files are collected and
//                scanned for credentials.  NOTHING the model wrote is built or run here.
//   verify job  (holds NO secret, fresh machine)  verify
//       verify   overlay the changed files, classify the diff against the policy and - only if it is not "blocked" -
//                build and run the tests.  Model-written code runs only here.
//
// Separate machines matter: code that runs in the verify job (a unit test the model wrote, say) cannot reach the
// credential through the process table, the environment, the file system or the runner's file commands.
// Nothing is pushed here; fix-publish.mjs (no model credential) opens the draft PR.

import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  appendFileSync, copyFileSync, existsSync, lstatSync, mkdirSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync,
} from 'node:fs';
import { dirname, join, relative } from 'node:path';

import {
  REPO, UNTRUSTED_NOTICE, api, assertNoSecrets, issueHash, readJson, runClaude, sanitizeForComment, secretsFromEnv, untrusted,
  writeJson,
} from './lib.mjs';
import { addedLinesOf, classifyChange } from './policy.mjs';

const mode = process.argv[2];
const WORK = 'agent-work';
const CHANGES = 'agent-changes';
const ROUND = Number(process.env.ROUND || 1);
const MAX_ROUNDS = Number(process.env.FIX_MAX_ROUNDS || 2);
const MAX_CHANGED_FILES = 40;
const MAX_FILE_BYTES = 2 * 1024 * 1024;

function sh(cmd, args, opts = {}) {
  return spawnSync(cmd, args, { encoding: 'utf8', maxBuffer: 256 * 1024 * 1024, ...opts });
}
const tail = (text, lines) => String(text).split('\n').slice(-lines).join('\n');
const sha256 = (path) => createHash('sha256').update(readFileSync(path)).digest('hex');
const setOutput = (k, v) => process.env.GITHUB_OUTPUT && appendFileSync(process.env.GITHUB_OUTPUT, `${k}=${v}\n`);

/** All regular files under dir (relative, forward slashes); throws on symlinks and other oddities. */
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

const safeRelative = (p) => !p.startsWith('/') && !p.split('/').some((s) => s === '..' || s === '.git' || s === '');

function copyInto(destRoot, srcRoot, files) {
  for (const f of files) {
    if (!safeRelative(f)) throw new Error(`unsafe path in agent output: ${f}`);
    if (statSync(join(srcRoot, f)).size > MAX_FILE_BYTES) throw new Error(`file too large: ${f}`);
    mkdirSync(dirname(join(destRoot, f)), { recursive: true });
    copyFileSync(join(srcRoot, f), join(destRoot, f));
  }
}

// ----------------------------------------------------------------------------------------------- prepare
function prepare() {
  const number = Number(process.env.ISSUE_NUMBER);
  if (!Number.isInteger(number) || number <= 0) throw new Error('ISSUE_NUMBER is required');
  const issue = api(`repos/${REPO}/issues/${number}`);
  if (issue.pull_request) throw new Error('not an issue');
  if (process.env.BODY_SHA && issueHash(issue.title, issue.body) !== process.env.BODY_SHA) {
    throw new Error('the issue was edited after it was approved for a fix attempt; stopping');
  }
  writeJson('fix-issue.json', { number, title: issue.title, body: issue.body ?? '' });

  rmSync(WORK, { recursive: true, force: true });
  mkdirSync(WORK);
  // Tracked files of the clean checkout, without .git.
  const list = sh('git', ['ls-files', '-z']);
  if (list.status !== 0) throw new Error('git ls-files failed');
  const paths = list.stdout.split('\0').filter(Boolean);
  const manifest = {};
  for (const p of paths) {
    if (!existsSync(p) || lstatSync(p).isSymbolicLink() || !lstatSync(p).isFile()) continue;
    mkdirSync(dirname(join(WORK, p)), { recursive: true });
    copyFileSync(p, join(WORK, p));
    manifest[p] = sha256(p);
  }
  writeJson('fix-manifest.json', manifest);

  // Continue from the previous round's edits, if any.
  if (existsSync('prev-changes')) {
    const prev = listFiles('prev-changes');
    copyInto(WORK, 'prev-changes', prev);
    console.log(`overlaid ${prev.length} files from the previous round`);
  }
  console.log(`round ${ROUND}: snapshot of ${paths.length} files`);
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
- Fix the defect only. Do not change how anything looks or reads: no new or reworded user-visible text, widgets, layout,
  styling, icons or .ui files. If the right fix needs that, set cannot_fix true and say so: a person decides.
- If the report is unclear, describes something that is not a bug, or you cannot fix it safely, set cannot_fix
  true and explain briefly. That is a good outcome.
- You cannot run the build; it is run for you after you finish, and failures are reported back to you.`;

async function agent() {
  const issue = readJson('fix-issue.json');
  const feedback = existsSync('prev-feedback.txt') ? readFileSync('prev-feedback.txt', 'utf8') : '';
  const model = process.env.FIX_MODEL || 'sonnet';
  const prompt = [
    UNTRUSTED_NOTICE, '', repoGuidance, '', rules, '', 'Fix this issue.',
    untrusted('issue-title', issue.title, 300),
    untrusted('issue-body', issue.body, 12000),
    feedback ? `\nYour previous attempt had problems:\n${untrusted('feedback', feedback, 6000)}\nFix them.` : '',
    '', 'When finished, answer with the JSON object requested (summary: what you changed and why, in plain words).',
  ].join('\n');

  console.log(`round ${ROUND}/${MAX_ROUNDS}: asking ${model}`);
  const result = await runClaude({
    prompt, model, tools: 'Read,Glob,Grep,Edit,Write', permissionMode: 'acceptEdits', schema: FIX_SCHEMA,
    cwd: WORK, maxBudgetUsd: Number(process.env.FIX_BUDGET_USD || 3), timeoutMs: 25 * 60 * 1000,
    system: 'You are a careful C++ maintainer fixing one defect. You can only read and edit files.',
  });
  const answer = result.structured ?? { cannot_fix: true, reason: 'no structured answer', summary: '', tests_added: false };

  // Collect what changed relative to the clean checkout, scanning for credentials while we still hold them.
  const secrets = secretsFromEnv('CLAUDE_CODE_OAUTH_TOKEN');
  const manifest = readJson('fix-manifest.json');
  const changed = [];
  for (const f of listFiles(WORK)) {
    if (manifest[f] === undefined || sha256(join(WORK, f)) !== manifest[f]) {
      assertNoSecrets(readFileSync(join(WORK, f), 'latin1'), secrets);
      changed.push(f);
    }
  }
  if (changed.length > MAX_CHANGED_FILES) throw new Error(`too many changed files (${changed.length})`);
  assertNoSecrets(JSON.stringify(answer), secrets);

  rmSync(CHANGES, { recursive: true, force: true });
  mkdirSync(CHANGES);
  copyInto(CHANGES, WORK, changed);
  writeJson('agent-answer.json', answer);
  console.log(`${changed.length} files changed`);
}

// ------------------------------------------------------------------------------------------------- verify
function currentChange(changed) {
  // Stage only the files the model changed (the checkout also holds build caches and other untracked files).
  sh('git', ['add', '--', ...changed]);
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
  return { files, patch, classification: classifyChange(files, addedLinesOf(patch), { diffChars: patch.length }) };
}

function buildAndTest() {
  const cpus = Number.parseInt(sh('nproc', []).stdout, 10) || 2;
  const build = sh('cmake', ['--build', 'build', '-j', String(cpus)]);
  if (build.status !== 0) return { ok: false, log: `BUILD FAILED\n${tail(build.stdout + build.stderr, 80)}` };
  // The unit tests are Qt programs: they need a display (CI uses xvfb-run too).
  const test = sh('xvfb-run', ['-a', 'ctest', '--test-dir', 'build', '--output-on-failure', '--timeout', '120']);
  if (test.status !== 0) return { ok: false, log: `TESTS FAILED\n${tail(test.stdout + test.stderr, 80)}` };
  return { ok: true, log: tail(test.stdout, 15) };
}

function finish(outcome) {
  const issue = readJson('fix-issue.json');
  writeJson('fix-result.json', { number: issue.number, title: issue.title, outcome });
  setOutput('status', outcome.status);
  console.log(`result: ${outcome.status}`);
}

/** Keep our own bookkeeping files out of "git add -A" (local exclude file). */
function excludeBookkeeping() {
  const exclude = '.git/info/exclude';
  const have = existsSync(exclude) ? readFileSync(exclude, 'utf8') : '';
  const patterns = ['/agent-changes/', '/agent-work/', '/agent-answer.json', '/fix-issue.json', '/fix-manifest.json', '/fix-result.json',
    '/fix.patch', '/feedback.txt', '/prev-changes/', '/prev-feedback.txt', '/commit-message.txt', '/pr-body.md'];
  mkdirSync(dirname(exclude), { recursive: true });
  writeFileSync(exclude, `${have}\n${patterns.join('\n')}\n`);
}

function verify() {
  excludeBookkeeping();
  const answer = readJson('agent-answer.json');
  if (answer.cannot_fix) {
    return finish({ status: 'cannot-fix', reason: sanitizeForComment(answer.reason ?? '', 400) });
  }

  // Overlay the agent's files onto the checkout (regular files only; it cannot delete anything).
  const changed = existsSync(CHANGES) ? listFiles(CHANGES) : [];
  copyInto('.', CHANGES, changed);

  const change = currentChange(changed);
  console.log(`change: ${change.files.length} files, risk ${change.classification.risk}`);
  let feedback = '';
  if (change.files.length === 0) {
    feedback = 'You made no changes.';
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
        attempts: ROUND,
      });
    }
    feedback = built.log;
  }

  if (ROUND >= MAX_ROUNDS) return finish({ status: 'failed', reason: sanitizeForComment(feedback, 600) });
  writeFileSync('feedback.txt', feedback);
  setOutput('status', 'continue');
  console.log('this round did not pass; the next round gets the feedback');
}

if (mode === 'prepare') prepare();
else if (mode === 'agent') await agent();
else if (mode === 'verify') verify();
else throw new Error(`unknown mode: ${mode}`);
