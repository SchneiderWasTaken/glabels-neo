interface Printer { name: string; description: string; enabled: boolean }

export class GlApp extends HTMLElement {
  async connectedCallback() {
    this.attachShadow({ mode: 'open' }).innerHTML = `
      <style>
        :host { display: block; font: 14px system-ui, sans-serif; padding: 1rem; }
        h1 { font-size: 1.2rem; } li.off { opacity: .5 }
      </style>
      <h1>gLabels-neo</h1>
      <h2>Printers</h2><ul id="printers"><li>Loading…</li></ul>
      <h2>Documents</h2><ul id="docs"><li>Loading…</li></ul>`;
    const [printers, docs] = await Promise.all([
      fetch('/api/printers').then((r) => r.json() as Promise<Printer[]>),
      fetch('/api/documents').then((r) => r.json() as Promise<string[]>),
    ]);
    const fill = (id: string, items: string[]) =>
      (this.shadowRoot!.getElementById(id)!.innerHTML = items.join('') || '<li>None</li>');
    fill('printers', printers.map((p) => `<li class="${p.enabled ? '' : 'off'}">${p.name}</li>`));
    fill('docs', docs.map((d) => `<li>${d}</li>`));
  }
}
customElements.define('gl-app', GlApp);
