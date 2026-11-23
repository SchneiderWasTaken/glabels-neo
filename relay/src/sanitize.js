// Input validation and Markdown-safe formatting for incoming reports.
//
// Everything in a report is untrusted: it is written into a public GitHub issue and later read by
// automation (including language-model agents).  The rules here keep a report from
//   - pinging people (@mentions),
//   - forging the machine-readable markers the automation relies on,
//   - breaking out of the code fences that hold diagnostics and attachments,
//   - exceeding GitHub's size limits.

export const LIMITS = {
  summary: 120,
  description: 8000,
  diagnostics: 4000,
  version: 80,
  attachmentName: 60,
  attachmentText: 60000,   // per attachment; GitHub comments allow 65536 chars
  attachments: 2,
  attachmentsTotal: 120000,
  bodyBytes: 300000,       // whole request
};

export const MARKER = '<!-- glabels-neo-report:v1 -->';

const ZWSP = '\u200b';

/** Remove control characters (keeps \n and \t), normalise newlines. */
export function cleanText(value) {
  return String(value ?? '')
    .replace(/\r\n?/g, '\n')
    // eslint-disable-next-line no-control-regex
    .replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f\u2028\u2029\u202a-\u202e\u2066-\u2069]/g, '')
    .trim();
}

/** Text that is rendered as Markdown (not inside a fence). */
export function neutralizeMarkdown(text) {
  return text
    // Break @mentions and team mentions so nobody is notified.
    .replace(/@(?=[A-Za-z0-9_])/g, '@' + ZWSP)
    // Stop forged HTML comments (our markers) and raw HTML openers.
    .replace(/<!--/g, '&lt;!--')
    .replace(/<(?=\/?[A-Za-z!?])/g, '&lt;');
}

/** A code fence long enough that the content cannot close it. */
export function fenceFor(text) {
  let longest = 0;
  for (const m of text.matchAll(/`+/g)) longest = Math.max(longest, m[0].length);
  return '`'.repeat(Math.max(3, longest + 1));
}

export function fenced(text, lang = 'text') {
  const f = fenceFor(text);
  return `${f}${lang}\n${text}\n${f}`;
}

function truncate(text, max) {
  if (text.length <= max) return text;
  return text.slice(0, max - 20) + '\n[truncated]';
}

export class ValidationError extends Error {
  constructor(message) {
    super(message);
    this.name = 'ValidationError';
  }
}

/**
 * Validate and normalise a report payload.  Throws ValidationError.
 * Returns { summary, description, diagnostics, version, attachments: [{name, kind, text}] }.
 */
export function validateReport(raw) {
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) throw new ValidationError('body must be an object');
  if (raw.schema !== 1) throw new ValidationError('unsupported schema');

  const description = truncate(cleanText(raw.description), LIMITS.description);
  if (description.length < 10) throw new ValidationError('description is too short');

  let summary = cleanText(raw.summary).replace(/\s+/g, ' ');
  if (!summary) summary = description.split('\n')[0].replace(/\s+/g, ' ');
  summary = truncate(summary, LIMITS.summary).replace(/\n.*/s, '');

  const diagnostics = truncate(cleanText(raw.diagnostics), LIMITS.diagnostics);
  const version = truncate(cleanText(raw.version).replace(/\s+/g, ' '), LIMITS.version);

  const attachments = [];
  let total = 0;
  const list = Array.isArray(raw.attachments) ? raw.attachments : [];
  if (list.length > LIMITS.attachments) throw new ValidationError('too many attachments');
  for (const a of list) {
    if (!a || typeof a !== 'object') throw new ValidationError('bad attachment');
    const kind = a.kind === 'log' ? 'log' : a.kind === 'label' ? 'label' : null;
    if (!kind) throw new ValidationError('unknown attachment kind');
    const text = truncate(cleanText(a.text), LIMITS.attachmentText);
    total += text.length;
    if (total > LIMITS.attachmentsTotal) throw new ValidationError('attachments too large');
    const name = cleanText(a.name).replace(/[^A-Za-z0-9._ -]/g, '_').slice(0, LIMITS.attachmentName) || kind;
    attachments.push({ name, kind, text });
  }
  return { summary, description, diagnostics, version, attachments };
}

/** Issue title and body for a validated report. */
export function formatIssue(report) {
  const title = `[Bug]: ${neutralizeMarkdown(report.summary)}`.slice(0, 200);
  const parts = [MARKER, '### What happened?', '', neutralizeMarkdown(report.description), ''];
  if (report.diagnostics) {
    parts.push('### Environment', '', fenced(report.diagnostics), '');
  }
  parts.push(
    '<sub>Filed from the in-app reporter with the reporter\'s consent' +
      (report.version ? ` &middot; ${neutralizeMarkdown(report.version)}` : '') +
      '. The text above is user-supplied and unverified.</sub>',
  );
  const comments = report.attachments.map((a) => {
    const lang = a.kind === 'label' ? 'xml' : 'text';
    const kb = Math.max(1, Math.round(a.text.length / 1024));
    return [
      `<details><summary>${a.kind === 'label' ? 'Label file' : 'Log'}: ${a.name} (${kb} KB)</summary>`,
      '',
      fenced(a.text, lang),
      '',
      '</details>',
    ].join('\n');
  });
  return { title, body: parts.join('\n'), comments };
}
