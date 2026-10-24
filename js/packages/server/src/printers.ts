import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const run = promisify(execFile);

export interface Printer { name: string; description: string; enabled: boolean; }

/** Lists CUPS destinations via `lpstat`. Returns [] if CUPS isn't installed. */
export async function listPrinters(): Promise<Printer[]> {
  try {
    const { stdout } = await run('lpstat', ['-p']);
    return stdout.split('\n').flatMap((line) => {
      const m = /^printer (\S+) (.*)$/.exec(line);
      return m ? [{ name: m[1], description: m[2], enabled: !/disabled/.test(m[2]) }] : [];
    });
  } catch {
    return [];
  }
}
