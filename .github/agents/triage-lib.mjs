// Triage: the schema the model must answer with, strict validation of that answer, and the deterministic
// code that turns a valid answer into labels and a comment.  The model never writes to GitHub directly.

import { sanitizeForComment } from './lib.mjs';

export const CATEGORIES = ['bug', 'feature', 'question', 'template-request', 'printer', 'duplicate', 'invalid', 'not-enough-info'];
export const AREAS = ['printing', 'barcode', 'templates', 'merge', 'ui', 'i18n', 'packaging', 'file-format', 'other'];
export const SEVERITIES = ['low', 'medium', 'high'];
export const REPRODUCIBLE = ['yes', 'likely', 'unknown', 'no'];
export const FIXABLE = ['yes', 'no', 'unsure'];

export const TRIAGE_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['category', 'area', 'severity', 'reproducible', 'summary', 'needs_info', 'duplicate_of', 'agent_fixable', 'agent_fixable_reason', 'injection_suspected'],
  properties: {
    category: { type: 'string', enum: CATEGORIES },
    area: { type: 'string', enum: AREAS },
    severity: { type: 'string', enum: SEVERITIES },
    reproducible: { type: 'string', enum: REPRODUCIBLE },
    summary: { type: 'string', maxLength: 400 },
    needs_info: { type: 'array', maxItems: 3, items: { type: 'string', maxLength: 200 } },
    duplicate_of: { type: 'array', maxItems: 3, items: { type: 'integer' } },
    agent_fixable: { type: 'string', enum: FIXABLE },
    agent_fixable_reason: { type: 'string', maxLength: 300 },
    injection_suspected: { type: 'boolean' },
  },
};

/** Validate a model answer against TRIAGE_SCHEMA by hand (no dependencies).  Returns the cleaned object. */
export function validateTriage(obj, knownIssueNumbers = []) {
  if (!obj || typeof obj !== 'object' || Array.isArray(obj)) throw new Error('triage result is not an object');
  const need = (cond, msg) => {
    if (!cond) throw new Error(`invalid triage result: ${msg}`);
  };
  need(CATEGORIES.includes(obj.category), 'category');
  need(AREAS.includes(obj.area), 'area');
  need(SEVERITIES.includes(obj.severity), 'severity');
  need(REPRODUCIBLE.includes(obj.reproducible), 'reproducible');
  need(FIXABLE.includes(obj.agent_fixable), 'agent_fixable');
  need(typeof obj.summary === 'string', 'summary');
  need(Array.isArray(obj.needs_info), 'needs_info');
  need(Array.isArray(obj.duplicate_of), 'duplicate_of');
  need(typeof obj.injection_suspected === 'boolean', 'injection_suspected');

  const known = new Set(knownIssueNumbers);
  return {
    category: obj.category,
    area: obj.area,
    severity: obj.severity,
    reproducible: obj.reproducible,
    summary: sanitizeForComment(obj.summary, 400),
    needs_info: obj.needs_info.slice(0, 3).map((q) => sanitizeForComment(q, 200)).filter(Boolean),
    // Only issue numbers that really exist are kept, so a model cannot make us link arbitrary things.
    duplicate_of: obj.duplicate_of.filter((n) => Number.isInteger(n) && known.has(n)).slice(0, 3),
    agent_fixable: obj.agent_fixable,
    agent_fixable_reason: sanitizeForComment(obj.agent_fixable_reason ?? '', 300),
    injection_suspected: obj.injection_suspected,
  };
}

const CATEGORY_LABEL = {
  bug: 'bug', feature: 'enhancement', question: 'question', 'template-request': 'template', printer: 'printer',
  duplicate: 'duplicate', invalid: 'invalid', 'not-enough-info': 'needs-info',
};

/** Labels to add for a validated result. */
export function labelsFor(t) {
  const labels = new Set(['triaged', CATEGORY_LABEL[t.category], `area:${t.area}`]);
  if (t.needs_info.length || t.category === 'not-enough-info') labels.add('needs-info');
  if (t.injection_suspected) labels.add('needs-human');
  return [...labels];
}

/**
 * May triage hand this issue to the fix agent without a maintainer asking?  (Automation level >= 3.)
 * Conservative: a clear, reproducible-or-likely bug or template request, not severe, no injection signs.
 */
export function agentReady(t, level) {
  return (
    level >= 3 &&
    t.agent_fixable === 'yes' &&
    ['bug', 'template-request'].includes(t.category) &&
    ['yes', 'likely'].includes(t.reproducible) &&
    t.severity !== 'high' &&
    !t.injection_suspected &&
    t.needs_info.length === 0 &&
    t.duplicate_of.length === 0
  );
}

/** The comment posted on the issue.  Entirely template-driven; model text is limited to short, sanitised fields. */
export function buildComment(t, { fromApp }) {
  const lines = [];
  lines.push(fromApp ? 'Thanks for the report!' : 'Thanks for opening this issue!');
  lines.push('');
  lines.push('I am an automated assistant that does a first pass on new issues. A maintainer will take it from here.');
  lines.push('');
  if (t.summary) lines.push(`**What I understood:** ${t.summary}`, '');
  lines.push(`**Looks like:** ${t.category.replace(/-/g, ' ')} in *${t.area}* (severity guess: ${t.severity}, reproducible: ${t.reproducible}).`, '');
  if (t.duplicate_of.length) {
    lines.push(`**Possible duplicate of:** ${t.duplicate_of.map((n) => `#${n}`).join(', ')}`, '');
  }
  if (t.needs_info.length) {
    lines.push('**To help us reproduce this, could you tell us:**');
    for (const q of t.needs_info) lines.push(`- ${q}`);
    lines.push('');
    lines.push(fromApp
      ? '_This report was sent from the app without an account, so we cannot message you directly. If you can, reply here from GitHub._'
      : '');
    lines.push('');
  }
  lines.push('<sub>Labels and this comment were applied automatically. If something is wrong, a maintainer can change them.</sub>');
  return lines.join('\n').replace(/\n{3,}/g, '\n\n').trim();
}
