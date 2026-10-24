import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildServer } from './index.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const app = buildServer({
  dataDir: process.env.GLABELS_DATA ?? path.resolve(here, '../../../../test-data'),
  webDir: process.env.GLABELS_WEB ?? path.resolve(here, '../../web/dist'),
});
await app.listen({ port: Number(process.env.PORT ?? 8080), host: process.env.HOST ?? '0.0.0.0' });
