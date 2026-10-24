# gLabels-neo (web / Electron)

TypeScript port that runs as a headless server + web UI (target: Raspberry Pi managing local
CUPS label printers), with Electron as a thin desktop shell around the same server.
The Qt/C++ app in the repo root stays as the reference implementation.

| Package | Role |
|---|---|
| `packages/core` | `.glabels` parsing / label model (TypeScript, no Qt) |
| `packages/server` | Fastify API: documents, CUPS printers (`lpstat`), serves the web UI |
| `packages/web` | Vite UI built from standard web components (Open UI approach) |
| `packages/electron` | Desktop shell: starts the server on localhost and opens a window |

```
cd js && npm install
npm run build && npm test
GLABELS_DATA=../test-data node packages/server/dist/main.js   # http://localhost:8080
npm run electron                                              # desktop window
```

## Roadmap
1. Rendering: label objects -> SVG (editor canvas + preview), barcodes via bwip-js.
2. Merge/Fill: port CSV/TSV/Excel data sources and the Fill table.
3. Printing: PDF generation server-side, submit to CUPS (`lp`), raw ZPL/ESC-P options.
4. Editor UI, template database (`templates/*.xml`), Deploy/kiosk mode as a route.
5. Pi packaging: systemd unit, Docker image / .deb.
