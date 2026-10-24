import { app, BrowserWindow } from 'electron';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildServer } from '@glabels/server';

// The desktop app is a thin shell: it runs the same server the Raspberry Pi runs, bound to localhost.
const here = path.dirname(fileURLToPath(import.meta.url));

app.whenReady().then(async () => {
  const server = buildServer({
    dataDir: path.join(app.getPath('documents'), 'glabels'),
    webDir: path.resolve(here, '../../web/dist'),
  });
  await server.listen({ port: 0, host: '127.0.0.1' });
  const { port } = server.server.address() as { port: number };
  const win = new BrowserWindow({ width: 1200, height: 800 });
  await win.loadURL(`http://127.0.0.1:${port}`);
  app.on('window-all-closed', () => { server.close(); app.quit(); });
});
