# LabelPi — design (working title)

Status: draft v0.2. This seeds a possible new repo; see "Repo plan" at the end.

A Raspberry Pi image that turns any printer (a Zebra off eBay, a Walmart Canon,
a used copier) into a shared, self-explaining print box with label design,
fill-and-print kiosks, automation hooks and fleet coordination built in.

## 1. Principles

1. **The user is the first-class citizen.** Two people matter: the *installer*
   (non-technical, one time, probably on a phone) and the *operator* (sales
   associate, never sees setup). No CLI, no config files, no jargon on the main
   path. Every failure is a plain sentence plus a next action. Nothing is a
   dead end.
2. **CUPS is the foundation, not an afterthought.** Every printer becomes a
   shared CUPS queue (IPP/AirPrint) whether or not our label tools are used.
3. **One output path for everything:** label engine -> PDF -> CUPS. ZPL is a
   fast path for Zebras layered on top later, not the core.
4. **Single node first, fleet-ready from day one.** Every user-editable object
   is a versioned document with a hybrid logical clock, so replication can be
   added without a rewrite.
5. **Offline is normal.** Setup, printing, kiosks and clusters must work with
   no internet. Online features (OTA, reports, cloud data sources) degrade
   gracefully and queue.
6. **Agents are users too.** Everything the UI can do is available as
   structured API/MCP with plain-language, machine-readable diagnostics.
7. **Privacy by construction.** Label contents, data sources, credentials and
   network identifiers never leave the box except by explicit user action.

## 2. User journeys

### 2.1 Install (installer, phone in hand)
1. Flash image from GitHub Releases, power on, plug in printer(s).
2. Box creates a `LabelPi-xxxx` WiFi network; captive portal opens the setup UI.
3. Set admin password and time (the phone's clock is offered as the time
   source; Pis have no RTC), choose reporting consent (section 9).
4. Printer wizard (section 4). Join WiFi/LAN or stay in hotspot mode.
5. The printer prints a **welcome page**: address, QR codes, "print from your
   phone/laptop: it's already shared".

### 2.2 Add another unit
New box finds an existing LabelPi network by mDNS and offers **"Join My Shop?"**
It shows a 6-digit pairing code; an admin approves on an existing unit by
confirming the same code (like Bluetooth pairing). See section 6.

### 2.3 Daily operator
Open the kiosk URL (or a saved home-screen app). Pick label, fill fields or
search a data source, see the preview, tap Print. Nothing else is visible.

### 2.4 Something breaks
Status cards are green/yellow/red in plain language ("Shipping printer: out of
paper. Load paper, then tap Resume"). A single **Something's wrong** button
runs the checks, fixes what is safe, otherwise produces a diagnostic bundle for
a human or an agent.

## 3. Architecture

```
 Clients: browser UI · kiosk · REST/MCP/webhooks · any LAN device (AirPrint/IPP)
                               │
  ┌────────────────────────────┴──────────────────────────────┐
  │ labelpid (our daemon)                                      │
  │  label engine · kiosk · data sources · printer registry &  │
  │  wizard · job router · notifications/webhooks · updater ·  │
  │  issue reporter · cluster agent · MCP/REST                 │
  └────────────────────────────┬──────────────────────────────┘
                               │ every job is a CUPS job
  ┌────────────────────────────┴──────────────────────────────┐
  │ CUPS + ipp-usb + avahi + cups-filters + bundled drivers    │
  └────────┬──────────────────┬───────────────────┬───────────┘
       Zebra (ZPL filter)  Canon/driverless    Copier (PCL/PS/IPP)
```

Proposed stack (open for revision): Go daemon (static arm64/armv7 binary),
SQLite, embedded web UI (Preact/Svelte + canvas editor), pi-gen to build the
image, RAUC (or equivalent) for A/B updates.

State lives on a separate persistent partition so OS/app updates never touch it.

## 4. Printer onboarding

Discovery is event-driven (USB hotplug, mDNS/IPP, SNMP); there is no "Add
printer" form. Flow per printer:

1. **Detect:** "Found: Canon PIXMA TS3320, connected by USB."
2. **Choose a driver:** driverless IPP -> matched bundled driver -> download if
   online. Bundle the common families (Gutenprint, foomatic, HPLIP, ...).
3. **Verify with a human:** print a test page, ask "Did a page come out? Does it
   look right?" Nothing -> checklist (paper, power, cable, paused). Garbled ->
   try the next candidate driver and ask again. Ends in a working driver or an
   honest "this printer can't be supported, here is why".
4. **Name and media:** "Shipping station"; pick loaded media from pictures
   (4x6 roll, Avery sheet, letter). Sheet printers get a calibration page whose
   x/y offsets are saved per printer.
5. **Classify and record** the outcome (section 9 uses this for unknown
   hardware).

Printer classes and honesty: driverless IPP and PCL/PS copiers and Zebras are
expected to work; vendor-driver-only inkjets are best effort; GDI/host-based
printers (e.g. Canon CAPT) are detected and reported as unsupported rather than
left as broken queues.

## 5. Label engine and output

- Label document: JSON, versioned, stored as a replicable document.
- Import: `.glabels` XML (including `${Field}` substitution per
  `docs/SUBSTITUTION-FIELD-SPEC.md`) and the 1,784 stock definitions in
  `templates/*.xml`.
- Renderer: layout -> PDF at exact media geometry (sheet labels, rolls,
  continuous). The preview shown to the user is that same PDF rasterized, so
  preview equals print.
- Barcodes/QR: generated in the engine; ZPL fast path may use native `^B*`
  commands later.
- Data sources: connector interface. v1: CSV/XLSX upload, Google Sheets, REST
  JSON. Each caches a last-good snapshot so kiosks survive outages.
- Kiosk: `/k/<id>` with a locked label, locked fields, optional PIN, scoped
  token; installable as a PWA.

## 6. Multi-unit coordination

Target: 2–20 units on one LAN. Design goals: zero config, any unit is a usable
entry point, no single point of failure for printing or editing, and no
dependence on quorum (the most common cluster is **two** units, where Raft
cannot survive a failure).

### 6.1 Identity and joining
- On first boot each unit generates a **node id** and keypair (never baked
  into the image).
- The first unit creates a **cluster** (id, human name such as "My Shop", and a
  cluster CA). A unit alone is a cluster of one.
- Discovery: avahi service `_labelpi._tcp` with TXT records: cluster id, node
  id, name, protocol version, role, priority. Fallback for networks where mDNS
  does not cross segments: "add unit by address".
- **Join is never automatic.** A new unit shows discovered clusters; the user
  picks one; both sides show a 6-digit code; an admin confirms on an existing
  unit; the cluster CA issues a node certificate. Joining adopts the cluster's
  replicated config (the joining unit offers an export of its own first).
  Merging two non-empty clusters is not supported in v1.
- **Cloned SD cards:** nodes detect duplicate node ids (mDNS conflict or
  heartbeat from a peer with the same id and different key). The later one
  regenerates its identity, leaves the cluster and offers "set up as a new
  unit?".
- All node-to-node traffic is mutual TLS using cluster certificates. Cert
  validity is generous and clock-tolerant (see 6.6).

### 6.2 Membership and health
Each node heartbeats its peers over HTTPS every 5 s; three misses = down. The
member list and each node's capability summary (Pi model, wired/WiFi, version,
attached printers) are replicated state. No gossip protocol is needed at this
scale.

### 6.3 Replicated state (multi-writer, no leader required)
Replicated: label designs, stock customizations, data-source definitions
(secrets encrypted with a cluster key), kiosk definitions, users/roles, API
tokens (hashed), notification and webhook config, printer registry metadata,
uploaded files (content-addressed blobs).

Local only: CUPS queue/driver config, WiFi credentials, logs and job history
(queryable cluster-wide by fan-out), device key, update state, data-source
caches (each node fetches on its own schedule).

Mechanism: documents carry a hybrid logical clock (HLC) and node id; sync is
pull-based anti-entropy every ~10 s plus a change nudge, last-writer-wins per
document with tombstones, and **every version is kept in a history table**, so a
clobbered edit is recoverable and a rare conflict is shown in the UI as "two
versions of this label — keep which?". This keeps editing working on a unit that
temporarily lost the network and converges when it returns.

### 6.4 Roles: a soft, stateless "primary"
The primary exists for convenience, not consistency. It: publishes the shared
name (default `my-labels.local`), is the default landing/UI redirect target,
sends cluster-level notifications (so N units do not send N emails), and
orchestrates rolling updates.

Election is a pure function of the visible membership, no voting:
1. user-pinned unit; else
2. static capability score (wired > WiFi, newer Pi > older, then stable uptime);
   else
3. lowest node id.

Hysteresis: the incumbent keeps the role unless it has been down for ~15 s or
the user re-pins. Live CPU load is deliberately **not** used (it causes
flapping). During a partition both sides may elect a primary: the worst case is
two units briefly answering for the shared name, which mDNS conflict handling
resolves on heal. Because the primary holds no unique state, nothing is lost.

Every unit serves the full UI, so the shared name is a convenience. Each unit is
also reachable at `labelpi-<id>.local`, every UI has a unit switcher, and the
welcome page QR encodes both the name and the IP. (Some Android browsers
resolve `.local` poorly; IP fallback is required.)

### 6.5 Printers and job routing
- A printer belongs to a **home node**: USB printers are bound to the unit they
  are plugged into; network (IPP) printers have a home node but may fail over to
  another unit automatically.
- The printer registry is cluster-wide: any unit's UI/API shows and can target
  every printer.
- A job submitted on any unit is rendered there to PDF, then forwarded to the
  owning unit's CUPS over standard **IPP** (units already share their queues, so
  no custom transport). Job state is tracked back via IPP job attributes.
- If the owning unit is down, the job is held in a durable queue on the
  submitter and shown as "waiting for Shipping station (offline)", with retry
  and expiry rules; failover-capable network printers are re-routed instead.
- Later: **federated queues** — every unit also exposes pass-through queues for
  all cluster printers so a laptop adds one printer server and sees everything.
  Filtering still happens only on the owning unit to avoid double processing.

### 6.6 Time without internet
Pis have no RTC and an offline LAN has no NTP. Rules: the HLC adopts the maximum
time seen so ordering stays sane after a bad boot clock; the primary (or any
unit with an RTC module) serves time to peers via chrony; setup and the admin UI
offer "set time from this device"; cluster certificates and update signature
verification must not depend on the wall clock.

### 6.7 Failure matrix
| Event | Behavior |
|---|---|
| Non-primary unit dies | Its USB printers go offline in the registry; a notification is sent by the primary; jobs for it queue. |
| Primary dies | After ~15 s the next-best unit takes the role and the shared name; no data loss. |
| Network partition | Both sides keep working; edits converge by LWW + history on heal. |
| New unit joins | Pairing-code approval, config sync, then it appears everywhere. |
| Unit decommissioned | "Forget unit" in UI; its printers marked removed; docs unaffected. |
| Version skew | Protocol N and N-1 interoperate; a larger gap shows "update unit X to keep syncing" and stops replicating with it. |

## 7. Notifications and webhooks

**Events** (examples): printer offline/back, out of media, low toner/ribbon,
job failed, unit down/back, update available, update overdue, report sent.

**Channels:** in-UI banner/inbox (always), email (SMTP), push (ntfy and similar),
chat webhooks (Slack/Teams/Discord formatters), generic webhook. Offline-only
sites rely on the in-UI inbox and on a **printed notice** (the box can print a
short "needs attention" page to a chosen printer).

**Rules:** per event and channel, severity thresholds, quiet hours.
Notifications fire on **state change**, with a re-notify interval, never per
repeated poll. Cluster-level events are sent by the primary only; unit-level
events by the owning unit (the primary reports a lost unit).

**Outbound webhooks:** JSON envelope
`{id, type, time, cluster, node, data}`, signed with HMAC-SHA256 in a header,
retry with exponential backoff, per-hook delivery log and a "send test" button
in the UI.

**Inbound:** `POST /api/v1/print` with template, data, printer, quantity and an
idempotency key, using a scoped token. A cloud relay can later deliver inbound
hooks through an outbound tunnel so no port needs opening.

## 8. Updates

### 8.1 Online (OTA)
- A/B root filesystems with automatic rollback if the new version does not
  confirm healthy, data on a separate persistent partition.
- Releases are **signed**; the device verifies against a pinned public key
  (with a planned rotation key). TLS alone is not trusted, and verification does
  not depend on the wall clock.
- Channels: stable (default) and beta. Apply in a maintenance window, never in
  the middle of a job.
- Schema migrations run after a pre-update backup of the data partition.
- Anti-rollback by default (monotonic version); explicit admin downgrade
  available for emergencies.

### 8.2 Offline units
The box tracks its last successful update check. After N days (default 30) it
shows an **update overdue** banner, notifies local channels, and can print a
reminder page. The admin has two options:

**A. Plug in a network cable for an hour.** On link-up the box checks
automatically, downloads and installs, shows progress and says when it is safe
to unplug.

**B. USB update.** On any PC, download the release bundle and its signature
from GitHub Releases onto a FAT32 stick under `labelpi/update/`. Insert it into
the Pi: the UI shows "Update found: v1.4 (you have v1.3) — install?", verifies
the signature, installs on admin confirm, reboots. The same stick convention
also carries:
- `labelpi/config/` — pre-seed WiFi/hostname/cluster join settings;
- `labelpi/outbox/` — queued issue reports and diagnostics exported for the
  admin to submit from a connected PC.

### 8.3 Fleet updates
- Rolling and ordered: secondaries first, primary last, one at a time, and only
  when a unit has no active job.
- **Update mesh:** any unit holding a newer verified bundle (from OTA or USB) can
  serve it to peers over the LAN, which re-verify the signature themselves. One
  USB stick, or one hour of internet on one unit, updates the whole fleet.

## 9. Automatic issue reporting

Goal: crashes, unknown hardware and unhandled configurations become GitHub
issues an agent can triage and fix for the next release.

### 9.1 Architecture
Units **never hold a GitHub credential** (an SD card can be read by anyone).
Instead:

`unit -> report relay (small service) -> GitHub App -> intake repo issue`

- Each unit signs reports with its device key. The relay registers devices
  (trust-on-first-use), applies per-device and global rate limits and a daily
  cap, validates the schema, **dedups by fingerprint**
  (`hash(error class + normalized stack + hardware ids + version)`) and creates
  or updates a single issue per fingerprint with an occurrence count.
- The GitHub App is held only by the relay, with issues-write on the intake
  repo only.
- Reports queue on disk when offline and flush on connectivity, or are exported
  to the USB `outbox/` (section 8.2).
- **Closing the loop:** when a fix ships, the relay tells units that reported
  the fingerprint "fixed in v1.4", and the UI shows "an update fixes this".

### 9.2 What triggers a report
Daemon crash/panic; failed update or rollback; a printer for which no driver
candidate was confirmed; a never-seen hardware/config combination; repeated
unrecoverable job failures; any condition tagged "unhandled" in code.

### 9.3 What is in a report (and what is never)
Included: version, Pi model, OS/driver package versions, printer USB
vendor:product ids, IEEE 1284 device id string, IPP make-and-model and
capability attributes, drivers tried and the user's pass/fail answers, error
codes, path-scrubbed stack trace, an install id that is a salted hash.

Never included: label designs or contents, data-source rows or credentials,
SSIDs, IP/MAC/hostnames, serial numbers, shop/cluster names, free-text user
input, tokens, or logs beyond a scrubbed, size-capped excerpt.

### 9.4 Consent and control
- Explicit consent screen at setup that shows a real sample report and lets the
  user turn reporting off; changeable later, and cluster-wide.
- "View/Send/Discard" for any pending report in the UI.
- Because `glabels-neo` is public, raw intake goes to a **private** repo; only
  sanitized "known printer" entries are published to the public repo.

### 9.5 Agent safety
Report fields originate from untrusted devices and printers (model strings,
error messages). The relay renders them in fenced, typed JSON blocks and
sanitizes control characters; the triage agent treats issue bodies as data, not
instructions, and all fixes land via reviewed pull requests, never auto-merged.

## 10. Agent interface

- Every unit exposes REST (OpenAPI) and an MCP server, plus `/llms.txt`;
  tool calls on any unit can address the whole cluster.
- `diagnose_printer` merges IPP state reasons (`media-empty`, `toner-low`,
  `paused`), CUPS queue state, and ZPL `~HS`/`~HQES` for Zebras into structured
  findings, each with a remediation and a `safe_to_automate` flag. Findings that
  need hands ("head open") are returned as human actions.
- Safe actions: resume, clear queue, reprint last, restart CUPS, rescan. Scoped
  tokens (read / operate / admin) and an audit log for every action.

## 11. Security summary

- Setup forces an admin password; LAN-only exposure by default.
- CUPS sharing and the kiosk/API sit behind roles and scoped tokens.
- Node-to-node: mutual TLS with a cluster CA; join requires human approval.
- Update bundles are signature-verified; issue relay devices are rate-limited;
  inbound webhooks are signed.

## 12. What we reuse from glabels-neo

| Asset | Use |
|---|---|
| `templates/*.xml` (1,784 stock templates) | Imported stock catalog; sheet printing on inkjets/copiers. |
| `.glabels` format + substitution spec | Import/export; design on desktop, upload to the box. |
| Fill/Deploy design | Becomes the kiosk model. |
| `PageRenderer` sheet-layout logic | Reference for sheet geometry and calibration. |
| `glbarcode` | Reference algorithms; the web renderer will use its own barcode libs. |

Code copied from this repo is GPL-3, so a new repo that copies code must be
GPL-3-compatible.

## 13. Build order

0. This document and a clickable setup-wizard prototype against fake printers,
   including failure branches.
1. Label JSON schema, `.glabels` importer, PDF renderer, CUPS submit, on an x86
   Linux dev box/container. Documents are versioned with HLC from the start.
2. Real printer wizard + discovery + driver candidate loop; Pi image (pi-gen),
   first boot, hotspot/captive portal, time handling.
3. Kiosk, data sources, notifications, webhooks, REST API.
4. Updater: A/B, signed releases, USB update, update-overdue flow.
5. Issue reporter + relay + intake repo + triage agent loop (earlier than usual
   on purpose, so beta units feed the loop).
6. Cluster: pairing, replication, primary role, job routing, update mesh.
7. MCP server and agent diagnostics hardening.
8. Zebra ZPL fast path and federated queues.

## 14. Open questions and risks

- Stack choice (Go vs Python) is a recommendation, not a decision.
- Real test hardware (exact Zebra, Canon and copier models) not yet known.
- Driver coverage is heuristic; the human-verified candidate loop is the
  mitigation and the intake data will grow a known-good list.
- Image size vs offline driver coverage; first-boot time on a Pi Zero 2 W must
  be measured early.
- Whether scan/copy (SANE/eSCL) belongs in v1 (currently: later).
- Reporting default and legal review of the consent text.

## Repo plan

Seeded on branch `claude/zealous-hamilton-leynt4` of `glabels-neo`. When the
direction is confirmed this moves to its own repository; this repo remains the
desktop designer and a source of templates and format specs.
