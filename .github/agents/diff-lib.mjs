// Parse a unified git diff into the structure policy.classifyChange() expects.

/**
 * Returns { files: [{ path, status, additions, deletions, binary, mode }], addedLines }.
 * A rename appears as the removal of the old path plus the addition of the new one, so that both sides are
 * checked against the path policy.
 */
export function parseDiff(diffText) {
  const files = [];
  const addedLines = [];
  const blocks = String(diffText).split(/^diff --git /m).slice(1);

  for (const block of blocks) {
    const lines = block.split('\n');
    const header = /^a\/(.+?) b\/(.+)$/.exec(lines[0]);
    if (!header) throw new Error('unparseable diff header');
    let oldPath = header[1];
    let newPath = header[2];
    let status = 'modified';
    let mode;
    let binary = false;
    let additions = 0;
    let deletions = 0;
    let inHunk = false;

    for (const line of lines.slice(1)) {
      if (inHunk) {
        if (line.startsWith('+')) {
          additions++;
          addedLines.push(line.slice(1));
        } else if (line.startsWith('-')) {
          deletions++;
        }
        continue;
      }
      if (line.startsWith('@@')) {
        inHunk = true;
      } else if (line.startsWith('new file mode ')) {
        status = 'added';
        mode = line.slice('new file mode '.length).trim();
      } else if (line.startsWith('deleted file mode ')) {
        status = 'removed';
      } else if (line.startsWith('new mode ')) {
        mode = line.slice('new mode '.length).trim();
      } else if (line.startsWith('rename from ')) {
        status = 'renamed';
        oldPath = line.slice('rename from '.length);
      } else if (line.startsWith('rename to ')) {
        newPath = line.slice('rename to '.length);
      } else if (line.startsWith('Binary files') || line.startsWith('GIT binary patch')) {
        binary = true;
      }
    }

    if (status === 'renamed') {
      files.push({ path: oldPath, status: 'removed', additions: 0, deletions, binary, mode: undefined });
      files.push({ path: newPath, status: 'added', additions, deletions: 0, binary, mode: undefined });
    } else {
      files.push({ path: status === 'removed' ? oldPath : newPath, status, additions, deletions, binary, mode });
    }
  }
  return { files, addedLines };
}
