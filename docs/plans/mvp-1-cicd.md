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

Naming: `glabels-neo-<version>-<os>-<arch>.<ext>` (stable) - consistent so that
"latest" download links and package manifests can be generated.

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
