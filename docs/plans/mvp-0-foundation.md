# MVP 0 - Foundation

**Goal:** turn the repository into a solid, license-clean, upstream-syncable base
that the CI/CD (MVP 1), site (MVP 2), automation (MVP 3) and features (MVP 4)
can be built on safely.

**Guiding principles**

1. *Small diff against upstream.* Our additions live in new files/directories;
   edits to upstream files are minimal. This keeps `merge upstream/master` cheap.
2. *Never rename internals.* Binaries, CMake targets, namespaces, `.glabels`
   format and MIME type stay. Only user-visible branding changes.
3. *Attribution travels with the name.* Wherever the product name "gLabels Neo"
   is shown, a short attribution to gLabels / Jaye Evins is shown nearby.
4. *Everything is reversible.* The pre-rebase state is preserved
   (`archive/legacy-master`, tags `v1.0.0`-`v1.2.0`, local backup mirror).

## Workstreams

### 0.1 History: rebase onto upstream - DONE
- 7 fork commits replayed linearly onto upstream `master` (`37901c1`),
  keeping original authorship. Conflicts were limited to README and
  `build-tests.yml` (both rewritten anyway).
- Verified: upstream dark mode, vector icon themes, startup-dir setting and new
  templates are present; every `.cpp` is registered in CMake.
- Staged on `rebase/upstream-2026-11`; promoted to `main` only after CI is green.
- Old `master` kept as `archive/legacy-master`; old `claude/*` branches
  re-homed under `archive/`.

### 0.2 Legal and attribution - DONE (pending notices file)
- `CREDITS.md`: upstream credits restored verbatim (template submitters,
  translators, contributors) + fork section. Placeholder removed.
- GPL-3.0-or-later headers added to all 12 first-party files that lacked them.
- `docs/CHANGES-FROM-UPSTREAM.md`: GPLv3 s.5(a) modification notice.
- README: independent-fork disclaimer, upstream link, trademark disclaimer.
- `THIRD-PARTY-NOTICES.md`: generated from the license inventory (Gemini
  research task) and wired into release packaging in MVP 1.
- Open items tracked from the inventory: `glabels/qxlsx_compat` stub headers
  (derive from MIT QXlsx), icon provenance, template-database license statement.

### 0.3 Brand
- **Name policy:** "gLabels Neo" for the product; never bare "gLabels" for our
  build. Always with attribution: *"A modernized fork of gLabels"* /
  *"Based on gLabels by Jaye Evins and contributors"*.
- Logo and SVG brand kit (purple-to-blue gradient) in `brand/`: mark, lockups,
  app icon, favicon, social card, design tokens. All original artwork.
- Display-name + attribution constant in one central place; applied to About
  dialog, window title, desktop entry, AppStream metainfo, `--version`, PDF
  producer. No translation churn (see rebrand audit).
- Side-by-side install safety: distinct app ID / bundle identifier /
  desktop-file name from upstream so both can be installed.

### 0.4 Repository governance
- Default branch `main`; ruleset: no force-push/deletion, required status
  checks (added when MVP 1 lands), linear history, squash merge.
- `CODEOWNERS`, issue forms (bug / feature / template request / printer
  compatibility), PR template, `SECURITY.md`, updated `CONTRIBUTING`,
  Dependabot (actions + submodules/fetched deps), labels, topics, description.
- Conventional Commits for release automation.

### 0.5 Upstream sync
- `upstream` remote documented; weekly workflow opens a PR merging
  `upstream/master` into `main` (conflicts labelled for a human/agent).
- Policy: generally useful fixes are kept on small topic branches so they can be
  offered upstream; fork-only features stay isolated.

### 0.6 Developer experience
- `.editorconfig`, existing `.clang-format` enforced in CI (MVP 1).
- Build/run docs verified on Windows (this machine), Linux, macOS.

## Exit criteria
- [ ] `main` == rebased + foundation commits; CI green on all three platforms
- [ ] Old history reachable via `archive/legacy-master` and tags
- [ ] No placeholder text; every first-party source file has a license header
- [ ] Name+attribution rule applied in the app and in the repo metadata
- [ ] Logo/brand kit committed under `brand/`
- [ ] Governance files present; ruleset active
- [ ] Roadmap and plans for MVP 1 committed
