# LabelOS — design

Status: draft v0.3. Staged in `glabels-neo`; destined for the `labelos-image`
repository.

A Raspberry Pi 4 image that turns any printer (a Zebra off eBay, a Walmart
Canon, a used copier) into a shared, self-explaining print box with simple label
design, fill-and-print kiosks, automation hooks and multi-unit coordination.
Free and open source; also sold as a ready-to-use boxed unit.

## 1. Principles

1. **The user is the first-class citizen.** The *installer* (non-technical, once,
   probably on a phone) and the *operator* (sales associate, touch screen).
   No CLI, no config files, no jargon on the main path. Every failure is a plain
   sentence plus a next action. Nothing is a dead end.
2. **Simplicity beats features.** Fewer lines means faster changes and faster
   agent fixes. Use CUPS, avahi, NetworkManager, chrony and systemd; do not
   rewrite them. Use standards (IPP, mDNS) instead of inventing protocols. One
   language, one database, one process. A line budget for the daemon is enforced
   in CI.
3. **CUPS is the foundation.** Every printer is a shared CUPS queue (IPP /
   AirPrint) whether or not the label tools are used.
4. **Offline is normal.** Setup, printing, kiosks and clusters work with no
   internet. Online features queue and degrade gracefully.
5. **Agents are users and maintainers.** Everything the UI can do is available
   as structured API/MCP, and the field-report-to-release loop is designed for
   agents, with a human approving every release.
6. **Privacy by construction.** Label contents, data sources, credentials and
   network identifiers never leave the box except by explicit user action.

## 2. Target hardware and stack

- **Raspberry Pi 4, 1–2 GB**, one supported board. Hardware watchdog via
  systemd (`RuntimeWatchdogSec`, `WatchdogSec` on the daemon). Built-in WiFi and
  Bluetooth LE. Pi 3 and Zero are out of scope.
- **Base:** Raspberry Pi OS Lite 64-bit, image built with pi-gen in GitHub
  Actions and published to Releases (`.img.xz`).
- **Daemon:** Python 3, one process (`labelosd`), SQLite, typed code, small
  modules. UI is server-rendered HTML with htmx; no JS build toolchain.
- **System services we depend on, not replace:** CUPS, avahi, NetworkManager,
  BlueZ, chrony, ipp-usb, systemd.
- **Memory budget:** idle under ~350 MB on a 1 GB board (CUPS + daemon +
  avahi). A dedicated-display mode (Chromium kiosk) is optional and off by
  default because of its memory cost.
- **Storage:** high-endurance SD (or eMMC/USB boot in product), logs in RAM,
  persistent data on its own partition so updates never touch it.

## 3. Architecture

```
 Clients: browser/phone/tablet kiosk · REST/MCP/webhooks · any LAN device (AirPrint/IPP)
                               │
  ┌────────────────────────────┴──────────────────────────────┐
  │ labelosd: label engine · kiosk · data sources · printer    │
  │ registry & wizard · history · notifications/webhooks ·     │
  │ updater · issue reporter · cluster agent · MCP/REST        │
  └────────────────────────────┬──────────────────────────────┘
                               │ every job is a CUPS job
  ┌────────────────────────────┴──────────────────────────────┐
  │ CUPS + ipp-usb + avahi + bundled drivers  [+ LPrint]       │
  └────────┬──────────────────┬───────────────────┬───────────┘
       Label printers      Canon/driverless     Copiers (PCL/PS/IPP)
```

## 4. Labels and rendering

- **A label is SVG.** A label document is a small JSON wrapper (size, DPI hint,
  element list, field names) that renders to SVG with `${Field}` values
  substituted. The browser preview is that SVG; printing rasterizes the same SVG.
- **Output modes:** PDF (vector) for page/sheet printers; a **1-bit PNG at the
  printer's exact DPI** for thermal printers. We write no ZPL/EPL emitter.
- **Barcodes/QR** are generated server-side as SVG with whole-dot module widths
  when the target DPI is known, so thermal scans are reliable.
- **Editor:** deliberately tiny, form-based. Pick a size; add text, barcode, QR,
  image and line elements with a few properties and a live preview. No freeform
  canvas. Complex designs are made in desktop gLabels and **imported**
  (`.glabels` XML including `${Field}` per `docs/SUBSTITUTION-FIELD-SPEC.md`);
  stock sizes come from the 1,784 definitions in `templates/*.xml`.
- **Data sources (v1):** CSV/XLSX upload and polling a URL (a published Google
  Sheet CSV link covers Sheets with no OAuth). Each caches a last-good snapshot.
- **Kiosk:** `/k/<id>`, touch-first for a tablet or phone over the LAN, or a
  dedicated display. Large targets, locked label and fields, optional PIN, idle
  reset between customers, scoped token, installable as a PWA.

## 5. Printers and the setup wizard

Discovery is event-driven (USB hotplug, mDNS/IPP, SNMP). There is no "Add
printer" form.

1. **Detect:** "Found: Canon PIXMA TS3320, connected by USB."
2. **Choose a driver:** driverless IPP, then a matched bundled driver, then
   LPrint for supported label printers, then on-demand download (below).
3. **Ask what it will be used for** and create queues to match:
   - labels designed here -> filtered queue;
   - documents from computers/phones -> filtered queue;
   - another system that already speaks the printer's language (e.g. ZPL from a
     warehouse system) -> additionally a **raw queue** (`name-raw`).
   Raw queues sniff the first bytes of each job and reject mismatches (a Zebra
   fed a PDF as raw data prints pages of garbage).
4. **Verify with a human:** test page, "Did it come out? Does it look right?"
   Nothing -> checklist (paper, power, cable, paused). Garbled -> try the next
   candidate driver and ask again. Ends in a working driver or an honest
   "this printer can't be supported, and here is why".
5. **Name and media:** "Shipping station"; loaded media chosen from pictures.
   Sheet printers get a calibration page; x/y offsets are saved per printer.
6. **Welcome page** printed on the printer: address, QR codes, "print from your
   phone or laptop, it's already shared".

**Non-free drivers** are never bundled. If a printer needs one, the wizard says
so plainly and offers two paths: "connect me to the internet" (fetched from the
vendor) or "download this file on another computer and put it in
`labelos/drivers/` on a USB stick". Printers whose vendor offers no arm64 driver
are reported as unsupported with the reason; GDI/host-based printers (e.g. Canon
CAPT) are detected and reported rather than left as broken queues.

### LPrint evaluation (docs-level, hardware test pending)
- Supports Zebra ZPL and EPL2, DYMO LabelWriter, TSPL, ESC/POS and Seiko SLP;
  Brother and CPCL are experimental. Input: raw, PWG/Apple raster, PNG (no PDF
  or JPEG). Each printer is an IPP Everywhere service. Apache-2.0, C, depends on
  PAPPL 1.2+ and CUPS 2.4+/libcups 3.
- Fit: excellent. Our 1-bit PNG output is exactly what it accepts, and it
  removes any need for us to write label-printer code.
- Risks: its README warns master is not suitable for packaging and no release is
  listed on the repo page, so we would build a pinned tag in the image build;
  single-maintainer project; not confirmed in Raspberry Pi OS repos. Decision:
  adopt as an **optional, pinned component** for label-class printers, with CUPS
  `rastertolabel` or a raw queue as fallback. Gated by spike S1.

## 6. History and reprint

CUPS keeps job history and files (`PreserveJobHistory`, `PreserveJobFiles`)
within caps on count, age and total size. LabelOS adds a friendly **History**
page over pycups: thumbnails, who/when/which printer, search, one-tap reprint.
It covers anything that went through the box, from any app. Access is an admin
setting: admin only, or also available on specific kiosks (e.g. "last 20 from
this kiosk"). A purge button and a "don't keep history" switch exist because
shipping labels contain customer data. History stays on the unit that printed;
cluster-wide search fans out to units. History is never included in reports.

## 7. Multi-unit coordination

Target: 2–20 units on one LAN. Goals: zero config, any unit is a usable entry
point, no single point of failure for printing or editing, no quorum
(the most common cluster is two units, where Raft cannot survive a failure).

### 7.1 Identity and joining
- Each unit generates a node id and keypair on first boot. The first unit
  creates a cluster (id, name such as "My Shop", cluster CA). A lone unit is a
  cluster of one.
- Discovery: avahi `_labelos._tcp` with TXT records (cluster id, node id, name,
  protocol version, role, priority). Fallback for segmented networks: add by
  address.
- All node-to-node traffic is mutual TLS from the cluster CA; certificate
  validity is generous and clock-tolerant.
- **Cloned SD cards:** a duplicate node id is detected; the later unit
  regenerates its identity and offers "set up as a new unit?".
- Merging two non-empty clusters is not supported in v1; joining adopts the
  cluster's config (the joiner offers an export first).

### 7.2 Joining a network with no admin window (Bluetooth LE)
Goal: a new unit joins with no extra step and no timed window.

1. An **unconfigured unit** (no cluster, no LAN) continuously scans for LabelOS
   BLE advertisements. A **configured unit** advertises a minimal beacon
   (protocol version only, no cluster details) while "allow new units" is on
   (default on, can be turned off).
2. They connect and authenticate with a **shared, predetermined PIN**. The new
   unit sends a join request (unit id, name, board, signal strength).
3. The request appears in the admin portal and as a notification: "New unit
   labelos-7f3a is asking to join (strong signal)". The admin approves or
   denies.
4. **Approve:** the existing unit sends the LAN's WiFi credentials (or the
   admin supplies them if the existing unit is wired-only) and a cluster join
   token over an encrypted channel. The new unit joins the LAN, completes the
   mTLS cluster join, and BLE drops off.
5. **Deny:** the unit id goes on a denied list, the new unit is told to stop
   trying that cluster, and the existing units ignore it until it is factory
   reset.

Honest security note: the PIN ships in open source and is therefore not a
secret. **The admin's approval is the real control.** To limit nuisance and
abuse: requests are rate-limited per unit, signal strength is shown, the admin
can switch "allow new units" off, and pending requests expire. This matches the
project's low-security threat model but should be reviewed before sale.

### 7.3 Ad-hoc and wired networks
- **Wired always allowed:** units on an Ethernet cable or plain switch with no
  DHCP get link-local addresses and find each other by mDNS.
- **WiFi without a router ("ad-hoc"):** true IBSS is poorly supported on the Pi
  and is not used. Instead one unit (chosen by the primary rule) hosts a
  hotspot and the others join it as clients. It is **gated by a connectivity
  test**: after joining, units run a mutual reachability check (repeated
  round-trip HTTPS pings both ways plus a short throughput burst). If loss,
  latency or throughput fall outside thresholds, ad-hoc mode is refused with a
  clear message ("these units cannot reliably talk over WiFi; use a cable or
  join a router") and the unit disconnects. The test reruns periodically and
  after each reconnect; a failing link demotes the unit to standalone.
  Client count is limited.

### 7.4 Membership and replicated state
- Heartbeats over HTTPS every 5 s between peers; three misses means down.
- **Replicated:** label designs, stock customizations, data-source definitions
  (secrets encrypted with a cluster key), kiosk definitions, users/roles, API
  tokens (hashed), notification/webhook config, printer registry metadata,
  uploaded files (content-addressed blobs).
- **Local only:** CUPS queues/drivers, WiFi credentials, logs, job history,
  device key, update state, data-source caches.
- **Mechanism:** documents carry a hybrid logical clock and node id; pull-based
  anti-entropy every ~10 s plus a change nudge; last-writer-wins per document
  with tombstones; every version kept in a history table so a clobbered edit is
  recoverable. No conflict-resolution UI in v1. A unit that loses the network
  keeps working and converges on return.

### 7.5 Roles and the shared name
A soft, stateless **primary** hosts `labelos.local` (configurable), is the
default landing target, sends cluster-level notifications, and orchestrates
rolling updates. Election is a pure function of visible membership: pinned unit,
else static capability score (wired over WiFi, stable uptime), else lowest node
id; the incumbent keeps the role unless it is down for ~15 s or the user re-pins.
Live load is deliberately not used. In a partition both sides may elect a
primary; because it holds no unique state, nothing is lost. Every unit serves
the full UI and is also reachable at `labelos-<id>.local`; QR codes carry both
the name and IP (some Android browsers resolve `.local` poorly).

### 7.6 Printers and routing
A printer belongs to a home node (USB printers are bound to it; network
printers may fail over). The printer registry is cluster-wide. A job submitted
on any unit is rendered there and forwarded to the owning unit's CUPS over
standard IPP. If the owner is down the job waits in a durable queue and the UI
says so. Federated pass-through queues are deferred beyond v1.

### 7.7 Time without internet
Pis have no RTC. The hybrid logical clock adopts the maximum time seen; the
primary serves time to peers via chrony; setup offers "set time from this
device"; certificates and update verification do not depend on wall-clock time.

### 7.8 Failure matrix
| Event | Behavior |
|---|---|
| Non-primary unit dies | Its USB printers go offline; primary notifies; jobs for it queue. |
| Primary dies | Next-best unit takes the role and name after ~15 s; no data loss. |
| Network partition | Both sides keep working; edits converge on heal. |
| Denied or unknown unit | Ignored; never auto-joined. |
| Unit decommissioned | "Forget unit"; printers marked removed. |
| Version skew | N and N-1 interoperate; larger gaps stop replicating and say which unit to update. |

## 8. Notifications and webhooks

- **Events:** printer offline/back, out of media, low toner/ribbon, job failed,
  unit down/back, update available or overdue, join request, report sent.
- **Channels:** in-UI inbox (always), email (SMTP), push (ntfy-style), chat
  webhooks (Slack/Teams/Discord formatters), generic webhook. Offline sites can
  print a "needs attention" page.
- **Rules:** per event/channel, severity, quiet hours. Fire on **state change**
  with a re-notify interval. Cluster events come from the primary only;
  unit-level events from the owning unit.
- **Outbound webhooks:** `{id, type, time, cluster, node, data}`, HMAC-SHA256
  signed, exponential-backoff retries, delivery log, "send test".
- **Inbound:** `POST /api/v1/print` (template, data, printer, quantity,
  idempotency key) with a scoped token; a cloud relay over an outbound tunnel can
  come later.

## 9. Updates

- **Online OTA:** A/B root filesystems with automatic rollback if the new
  version does not confirm healthy (RAUC or equivalent); data on its own
  partition; migrations run after a pre-update backup. Releases are signed and
  verified against a pinned public key (rotation key planned); TLS alone is not
  trusted, and verification does not depend on the clock. Channels: stable
  (default) and beta; applied in a maintenance window, never mid-job.
  Anti-rollback by default.
- **Offline units:** after N days (default 30) with no successful check, an
  **update overdue** banner, notification, and optional printed reminder.
  Options: *plug in a network cable for an hour* (the unit notices link-up,
  updates and says when to unplug), or the **USB procedure** below.
- **Fleet:** rolling and ordered (secondaries first, primary last, only when
  idle). **Update mesh:** any unit holding a newer verified bundle serves it to
  peers over the LAN, which re-verify the signature, so one stick or one hour
  online updates the whole fleet.

### USB toolkit
A FAT32 stick with a `labelos/` folder:
- `update/` — release bundle + signature -> "Update found: v1.4, install?";
- `drivers/` — user-supplied vendor drivers for the wizard;
- `config/` — pre-seed WiFi, hostname, cluster join;
- `outbox/` — exported diagnostics and queued reports to submit from a
  connected computer;
- `reset-admin` — an empty file that, with the stick inserted at boot,
  resets the admin password. (Physical access is the trust boundary; these are
  not high-security devices.)

## 10. Automatic issue reporting and the agent loop

### 10.1 Reporting pipeline
Units hold no GitHub credential. `unit -> relay -> GitHub App -> private intake
repo issue`. Reports are signed by a device key. The relay registers devices
(trust-on-first-use), **rate-limits per device and globally with a daily cap**,
validates the schema, dedups by fingerprint
(`hash(error class + normalized stack + hardware ids + version)`) and creates or
updates one issue per fingerprint with a count. Reports queue offline or export
to the USB `outbox/`.

**Triggers:** daemon crash; failed update or rollback; no confirmed driver for a
printer; never-seen hardware/config; repeated unrecoverable job failures; any
condition tagged "unhandled" in code; and a user-initiated **"I have a problem"**
button.

**Contents:** version, board, package/driver versions, USB vendor:product ids,
IEEE 1284 id, IPP make-and-model and capability attributes, drivers tried and
the user's pass/fail answers, error codes, scrubbed stack trace, salted-hash
install id. For "I have a problem", the user may add free text, always with a
preview and an explicit send. **Never:** label designs/contents, data rows,
credentials, SSIDs, IP/MAC/hostnames, serial numbers, shop or cluster names,
tokens, history, or unscrubbed logs.

**Consent:** an explicit setup screen with a real sample report; changeable
cluster-wide; view/send/discard for any pending report. Raw intake goes to a
**private** repo; only sanitized "known printer" entries are published.

**Opt-in heartbeat:** anonymous version and board counts, used for staged
rollouts and to measure whether a release made things better or worse.

### 10.2 From report to release
1. Intake issue (structured, fenced JSON; control characters stripped).
2. A triage agent (Jules, Copilot, Claude Code, ...) reproduces the problem with
   the **fake-printer simulator** and `labelos replay report.json`, then opens a
   PR. The agent treats issue content as untrusted data.
3. CI: unit tests, simulator scenarios, golden render tests, schema checks.
4. **Risk tiers:** data-only changes (e.g. a new printer-model entry) may
   auto-merge after schema validation; code changes need a reviewer agent;
   updater, cluster, security and signing code always need a human.
5. **A human approves every release.** Merging is not releasing; the agent has
   no access to the signing key. Approval is meant to be a minute of review of
   an agent-prepared release note and diff summary.
6. **Staged rollout:** canary, then beta, then stable, with automatic pause if
   crash-report rate rises.
7. **Close the loop:** the relay tells units that reported the fingerprint
   "fixed in v1.4"; the UI notifies the user. Offline units hear on their next
   update.
8. If the agent cannot fix it, the user gets "we're looking into it" and a human
   queue picks it up.

### 10.3 Making the repo agent-friendly
`AGENTS.md`; small modules; fast tests; the fake-printer simulator reused by the
wizard prototype and by CI; a `replay` command; a daemon line budget; a
changelog format that feeds release notes.

## 11. Agent interface

Every unit exposes REST (OpenAPI), an MCP server and `/llms.txt`; calls can
address the whole cluster. `diagnose_printer` merges IPP state reasons, CUPS
queue state and (via LPrint or raw status) label-printer status into structured
findings, each with a remediation and a `safe_to_automate` flag. Findings that
need hands ("head open") are human actions. Safe actions: resume, clear queue,
reprint last, restart CUPS, rescan. Scoped tokens (read / operate / admin) and
an audit log.

## 12. Product (boxed unit)

- Pi 4 in a case with a good PSU and high-endurance storage; target retail
  $99 or less (verify current Pi 4 prices before fixing).
- **NFC tag** with setup info; a unique per-unit setup-hotspot password on a
  sticker and in the tag (Android can read WiFi NFC records; others use the QR
  sticker). DIY images use the shorter setup window and a default PSK.
- Loose ends: FCC/CE and power-supply certification for assembled goods,
  warranty and returns, support load (the agent loop is the plan), trademark and
  domain check for the name.
- Only freely redistributable software is bundled.

## 13. Reuse from glabels-neo

| Asset | Use |
|---|---|
| `templates/*.xml` (1,784 definitions) | Stock catalog and sheet geometry. |
| `.glabels` format + substitution spec | Import from desktop gLabels. |
| Fill/Deploy design | Becomes the kiosk model. |
| `PageRenderer` sheet-layout logic | Reference for sheet geometry and calibration. |

Copied code is GPL-3, so the new repository must be GPL-3-compatible.

## 14. v1 scope and cut list

In: wizard, CUPS sharing, SVG labels + tiny editor + `.glabels` import, kiosk,
CSV/XLSX/URL data, history, notifications/webhooks, signed A/B updates + USB
toolkit, issue reporter and relay, clustering (BLE join, wired and gated
ad-hoc, replicated docs, routing), REST/MCP.

Out for v1: federated pass-through queues, conflict-resolution UI, scan/copy,
canvas editor, native ZPL/EPL emission, Pi 3 and Zero, enterprise WiFi.

## 15. Spikes before building

- **S1 LPrint on a Pi 4:** build a pinned tag, drive a real Zebra, confirm
  PNG-at-DPI quality and status reporting.
- **S2 Thermal crispness:** SVG -> 1-bit PNG at 203/300 dpi; text and barcode
  scan rates.
- **S3 BLE enrollment:** BlueZ GATT server/client with the PIN flow between two
  Pi 4s, including the deny path and range behavior.
- **S4 WiFi hub mode:** AP + clients on a Pi 4 and the connectivity test
  thresholds.
- **S5 A/B updates:** RAUC (or simpler) on Pi 4; power-loss behavior.
- **S6 Memory budget:** CUPS + daemon + avahi at 1 GB under load.

## 16. Build order

0. This document; fake-printer simulator and clickable wizard prototype.
1. Label JSON -> SVG -> PDF/PNG pipeline, `.glabels` importer, CUPS submit
   (on an x86 dev box/container); documents carry HLC from the start.
2. Real wizard, discovery and driver loop; Pi 4 image; first boot; time.
3. Kiosk, data sources, history, notifications, webhooks, REST.
4. Updater (A/B, signed, USB, overdue flow).
5. Issue reporter, relay, intake repo, triage agent loop (early, so beta units
   feed it).
6. Cluster: BLE join, replication, primary, routing, update mesh, ad-hoc test.
7. MCP and agent diagnostics hardening.
