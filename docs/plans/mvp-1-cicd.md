# MVP 1 - Robust CI/CD and "download and go" releases

**Goal:** any user can land on the Releases page, grab the build for their system
and run it - and every merge to `main` produces tested, verifiable artifacts
with no manual steps.

## User promise (acceptance test for the whole MVP)
> A person with a fresh Windows, macOS or Linux machine downloads one file,
> double-clicks / extracts it, and gLabels Neo opens and can print a label to
> PDF - with no compiler, no package hunting, no missing DLLs.

## Release model
| Channel | Trigger | Tag | Audience |
|---|---|---|---|
| **edge** | every merge to `main` | rolling `edge` prerelease (+ immutable `edge-<n>-<sha>` assets) | testers |
| **stable** | merging the auto-generated release PR (Conventional Commits -> SemVer, changelog) | `vMAJOR.MINOR.PATCH` | everyone |
- Version string baked into the app and artifact names from the git tag/describe.
- Stable releases are cut only from commits that already passed the full matrix.

## Artifacts (per release)
| Platform | Artifacts |
|---|---|
| Windows x64 | installer (`.exe`/`.msi`), portable `.zip`, kiosk SFX `.exe` |
| macOS | universal or arm64 + x86_64 `.dmg` (signed/notarized when credentials exist) |
| Linux x86_64 | AppImage (with update info), `.tar.gz`, Flatpak bundle, `.deb` |
| All | `SHA256SUMS`, SBOM (SPDX/CycloneDX), build provenance attestations, `THIRD-PARTY-NOTICES`, license folder, source tarball |

Naming: fixed, version-free file names (`glabels-neo-windows-x64.zip`, ...) so that
`releases/latest/download/<name>` links and package manifests never change; the version lives in the release.

## Workflows (one concern per file)
| File | Purpose |
|---|---|
| `ci.yml` | PR/push gate: build + unit tests matrix; fast, cached |
| `quality.yml` | clang-format check, clang-tidy/cppcheck, ASan/UBSan test run, warnings-as-errors on first-party code |
| `security.yml` | CodeQL (C++), dependency review, secret scan, OpenSSF Scorecard |
| `license.yml` | REUSE/SPDX header check, third-party license completeness, forbid disallowed licenses |
| `package.yml` | reusable workflow: builds every artifact for a platform |
| `smoke.yml` | installs/extracts each artifact in a clean environment, launches headless, renders a sample label to PDF and checks the output |
| `release.yml` | edge + stable publishing, checksums, SBOM, attestations, release notes (incl. GPLv3 source statement) |
| `release-please.yml` | maintains the stable release PR and changelog |
| `upstream-sync.yml` | weekly PR merging `upstream/master` |
| `dependabot.yml` | keeps Actions and fetched deps current |

### CI matrix (build + test)
- Linux: gcc + clang, Ubuntu LTS and latest, Qt 6.8 LTS and latest.
- Windows: MSVC (Qt official build) x64.
- macOS: Apple Silicon (and Intel if cheap).
- Offscreen Qt platform (`QT_QPA_PLATFORM=offscreen`) so GUI-dependent tests run headless.
- ccache/sccache + Qt install caching; concurrency groups cancel superseded runs.

### Required status checks on `main`
build (all matrix legs), tests, format, static analysis, CodeQL, license check, smoke.

## Supply-chain and compliance gates
- Artifacts must contain `LICENSE`, `THIRD-PARTY-NOTICES.md` and `licenses/`.
- Release notes include the GPLv3 corresponding-source statement pointing at the tag.
- Build provenance attestation (`actions/attest-build-provenance`) and `SHA256SUMS`.
- Pin third-party actions by SHA; least-privilege `permissions:` per job.

## "Drop in and go" distribution (follow-ups inside MVP 1 where feasible)
- Stable `releases/latest/download/<fixed-name>` links used by README and site.
- winget manifest, Homebrew cask (own tap), Scoop manifest; Flathub submission.
- Code signing: Windows via SignPath Foundation (apply; free for OSS - verify
  eligibility), macOS needs an Apple Developer ID for notarization (decision needed).
  Until signed, document the SmartScreen/Gatekeeper first-run steps.
- In-app "check for updates" pointing at the Releases API (optional, off by default).

## Work breakdown (parallelizable)
| # | Stream | Depends on |
|---|---|---|
| A | CI build/test matrix, caching, offscreen tests | rebased `main` |
| B | Quality + security + license workflows | A (for names) |
| C | Packaging: Windows installer/portable, macOS dmg, Linux AppImage/Flatpak/deb | A |
| D | Smoke/install tests per artifact | C |
| E | Release automation: edge + stable, checksums, SBOM, attestations, notes | C |
| F | Distribution manifests (winget/brew/scoop/flathub), latest-links | E |
| G | Governance: rulesets/required checks wired to final job names | A-E |

## Definition of done
- [ ] A PR cannot merge unless every required check is green
- [ ] Merge to `main` publishes an `edge` release with all artifacts in < 45 min
- [ ] Merging the release PR publishes a stable release with checksums, SBOM, attestations
- [ ] Each artifact passes the smoke test on a clean runner
- [ ] A non-developer completes the "user promise" test on Windows, macOS, Linux
- [ ] Flaky tests quarantined, not ignored; main stays green

## Status

### Wave 1 - pipeline foundation (implemented)
- `ci.yml`: PR gate on Linux (gcc, clang), Windows (MSVC) and macOS (arm64); Qt and compiler caching;
  stable aggregate status **CI OK**.
- `lint.yml`: actionlint, shellcheck and license-header/placeholder checks; aggregate status **Lint OK**.
- `release.yml`: build, test, package, **smoke-test the packaged files** (unpacked into a clean location,
  stripped environment, renders a label to PDF), then publish. Merges to `main` publish a rolling `edge`
  prerelease; `v*` tags publish a stable release. Both ship `SHA256SUMS`, build-provenance attestations and
  release notes with a "which file do I need?" table and the GPLv3 corresponding-source statement.
  PRs that touch packaging run it as a dry run (no publishing).
- Shared composite action `.github/actions/setup-build` so CI and release build against identical inputs.
- Packages now ship `LICENSE`, `THIRD-PARTY-NOTICES.md`, `CREDITS.md` and a `licenses/` folder.
- All third-party actions are pinned by commit SHA; Dependabot keeps them current.
- `.gitattributes` keeps shell scripts and workflows LF on Windows checkouts.

### Next waves
- **Wave 2 - installers and more platforms:** Windows installer (NSIS) next to the zip, Linux AppImage,
  Flatpak and `.deb`, macOS Intel/universal; icon pipeline from `brand/app-icon.svg`; kiosk SFX in the release.
- **Wave 3 - depth:** CodeQL, sanitizer (ASan/UBSan) test job, clang-tidy/cppcheck, clang-format enforcement,
  SBOM, license-allowlist check of bundled binaries, version pinning of Qt/vcpkg/zint.
- **Wave 4 - distribution and automation:** release-please (SemVer + changelog from Conventional Commits),
  winget/Homebrew/Scoop manifests, code signing (SignPath / Apple notarization) once accounts exist,
  weekly upstream-sync PR, required-status-check ruleset.

### Known gaps to close before the first stable release
Found by the wave 1 review and the license inventory; edge builds ship with these open, stable releases must not.
- **Third-party license texts:** packages ship the gLabels, glbarcode (LGPL), template (MIT) and QXlsx texts and
  Qt's `LICENSES/` when present, but not yet those of libzint (BSD-3-Clause), libqrencode (LGPL-2.1+), ICU
  (Unicode-3.0) and zlib. Collect them per platform from vcpkg / Homebrew / apt and add a CI check that fails
  when a bundled library has no license file.
- **`THIRD-PARTY-NOTICES.md` placeholders** (`<QT_VERSION>`, ...) must be filled from a build manifest, and the
  "being finalized" banner removed.
- **Windows translations:** fixed in packaging (gLabels' own `.qm` files are copied next to the executable);
  a code fix in `FileUtil` would be cleaner.
- **macOS:** `cmake/MacDeployFixup.cmake` may not rewrite every transitive Homebrew dylib; the new smoke test
  reports any leftover build-machine path. The apps are ad-hoc signed, not notarized.
- **Linux:** needs a documented system baseline (e.g. `libxcb-cursor0` for Qt 6.5+ on X11) or bundling; the
  informational bare-container step in `release.yml` reports what is missing.
- **Kiosk (`glabels-fill`):** no About/legal-notices dialog and no `scripts/build-sfx.ps1` in the repository.
