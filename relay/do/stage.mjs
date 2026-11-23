// Copies the relay sources next to the function entry point (the deploy packages only that directory).
import { cpSync, mkdirSync, rmSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const target = join(here, 'packages', 'relay', 'report', 'src');
rmSync(target, { recursive: true, force: true });
mkdirSync(target, { recursive: true });
cpSync(join(here, '..', 'src'), target, { recursive: true });
console.log(`staged relay sources into ${target}`);
// The sources are ES modules: say so for the directory the function is packaged from.
import { writeFileSync } from 'node:fs';
writeFileSync(join(target, 'package.json'), '{"type":"module"}\n');
