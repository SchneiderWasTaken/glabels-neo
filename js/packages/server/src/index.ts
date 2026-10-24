import { readdir, readFile } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';
import Fastify from 'fastify';
import fastifyStatic from '@fastify/static';
import { parseGlabels } from '@glabels/core';
import { listPrinters } from './printers.js';

export interface ServerOptions {
  /** Directory holding user .glabels files. */
  dataDir: string;
  /** Built web UI directory; served at / when present. */
  webDir?: string;
}

export function buildServer({ dataDir, webDir }: ServerOptions) {
  const app = Fastify({ logger: true });

  app.get('/api/printers', () => listPrinters());

  app.get('/api/documents', async () => {
    const files = existsSync(dataDir) ? await readdir(dataDir) : [];
    return files.filter((f) => f.endsWith('.glabels'));
  });

  app.get<{ Params: { name: string } }>('/api/documents/:name', async (req, reply) => {
    const name = path.basename(req.params.name);
    const file = path.join(dataDir, name);
    if (!name.endsWith('.glabels') || !existsSync(file)) return reply.code(404).send({ error: 'not found' });
    return parseGlabels(await readFile(file, 'utf8'));
  });

  if (webDir && existsSync(webDir)) app.register(fastifyStatic, { root: webDir });
  return app;
}
