# Automatic updates

gLabels Neo keeps itself up to date in the background. It checks for a newer release now and then,
downloads and checks it while you work, and swaps it in when you close the program, so the next
start is already on the new version. Nothing changes under you while the program is open.

## For users

- **The first time**, the program asks whether it may check for updates in the background. Until you say yes,
  it contacts nobody. *Don't Check* (or closing the question) is remembered and not asked again.
- **Help > Check for Updates...** looks right now, whatever you answered.
- **Edit > Preferences > Updates** turns the background check on or off at any time, or lets updates install without asking.
- When a newer version is found you are asked once: *Install in Background*, *Not Now* (asks again in a
  few days) or *Skip This Version*. The box *Install future updates automatically* makes later updates silent.
- Only the release channel you installed is followed: a **stable** release (`v1.2.0`, ...) follows stable
  releases; a build of the **edge** prerelease follows the newest build of `main`.
- What leaves your computer: once you have agreed to background checks, a request to `github.com` for a file of about 1 KB, at most every 12 hours
  (every 6 hours on edge), and the package download when you accept an update. GitHub sees your IP address, as
  with any download. Nothing else is sent, and there is no account or identifier.
- If an update cannot be installed, the version you are running is left exactly as it was. The log is
  `update.log` in the `updates` folder under the program's local app data folder
  (Windows: `%LOCALAPPDATA%\glabels.org\glabels-qt\updates`).

Where it works: the Windows zip, the Linux tarball and the macOS disk image, **when unpacked or copied somewhere
you can write to** (your home folder, `~/Applications`, `C:\Users\you\...`). It does not update:

- copies in places that need administrator rights (for example `C:\Program Files`); the Help menu says so;
- distribution packages and local developer builds;
- the copy managed by the [Windows launcher](../tools/windows-launcher/README.md), which has its own updater;
- versions from before this feature existed: install one release by hand, and updates are automatic from then on.

## How it works

```
 release workflow                          the program (running)                      after it exits
 ----------------                          ---------------------                      --------------
 publishes packages                        check    GET fixed manifest URL
 + glabels-neo-update-manifest.json   -->  offer    ask once (or auto, if opted in)
   (version, commit, per-platform          download package, verify size + SHA-256
    package with SHA-256)                  unpack   into hidden ".<name>.update" next to the install
                                           test     run the new copy with --Version
                                           record   pending.json                      glabels-updater swaps
                                                                                      the directories, restarts
                                                                                      the program if asked
```

### Finding new releases (no polling of the GitHub API)

Every release, stable and edge, carries one small file, `glabels-neo-update-manifest.json`, written by
`scripts/make-update-manifest.sh`. The program reads it from a fixed, public URL:

| Channel | URL |
|:--|:--|
| stable | `https://github.com/<repo>/releases/latest/download/glabels-neo-update-manifest.json` |
| edge | `https://github.com/<repo>/releases/download/edge/glabels-neo-update-manifest.json` |

This is a static file download, so there is no API rate limit, no token and no GitHub-specific JSON to parse,
and the format is ours (`schema` 1). It names the release tag, the commit the packages were built from, and for
each platform the package URL, size and SHA-256. The program refuses anything whose download URL is not a
release asset of this repository on `github.com` over HTTPS.

*Is a release newer?* Stable: the manifest's tag is a higher version number than the running build's tag.
Edge: the manifest's commit differs from the commit the running build was made from.

### Installing in the background

All of this runs asynchronously while the program is in use, without blocking the interface:

1. **Download** the package for this platform to the updater's data folder; size and SHA-256 must match the manifest.
2. **Unpack** next to the install into `.<name>.update` (Windows `tar.exe`, Linux `tar`, macOS `hdiutil` + `ditto`).
   Staging on the same volume is what makes the final step an atomic rename.
3. **Check** that the copy is complete and that no link points outside it, then **run it** (`--Version`) to prove it
   starts on this machine. A copy that fails is deleted and that release is not retried automatically.
4. **Record** it in `pending.json`.

### Switching over

The running program cannot replace its own files, so a small helper, `glabels-updater` (standard library only,
no Qt), does the last step. It is started from a *copy outside the install folder* so it is not itself replaced:

- **When the program exits** it is started to wait for that process to end, then rename
  `install -> .install.old` and `.install.update -> install`. Directory renames are atomic and take
  milliseconds. If anything fails everything done so far is rolled back. On Windows a rename can fail briefly
  while a virus scanner holds a file, so it retries for about 20 seconds.
- **At the next start**, if the update is still waiting (the program was killed, the machine lost power), the
  program hands over to the helper first thing, which swaps and starts it again with the same command line, so
  a double-clicked `.glabels` file still opens. A swap that fails twice is abandoned, not looped on.

On macOS both applications of the disk image (`glabels-qt.app`, `glabels-fill.app`) are swapped together.

## For maintainers

- **Which build follows which channel** is decided when it is compiled (`cmake/UpdateChannel.cmake`,
  `GLABELS_UPDATE_CHANNEL=auto|stable|edge|off`). `auto`: a build of an exact release tag follows *stable*; the
  push-to-`main` run of the release workflow follows *edge*; everything else (your own builds, pull
  requests) never updates itself. `GLABELS_UPDATE_REPO` sets the repository (default: this one).
- **A package must contain** the updater next to the main program (`bin/glabels-updater[.exe]`, or in
  `glabels-qt.app/Contents/MacOS`) and, for the smoke test, `glabels-batch-qt` (Windows `bin/`, Linux package
  root; macOS uses the app itself). The CMake install rules and the packaging scripts already arrange this.
- **Changing the manifest** means changing `scripts/make-update-manifest.sh` and `update/UpdateManifest.cpp`
  together; `TestUpdateManifest` runs the script and parses its output.
- **Trust.** Integrity comes from TLS to `github.com` plus the SHA-256 in the manifest, which stops corruption
  and truncation but not a compromised release. The packages are not code-signed yet. Signing the manifest
  with a key whose public half is compiled into the program is the next step if that matters.
- **Tests** (`update/unit_tests`): manifest parsing and version rules; install-layout detection per OS; the helper
  against real directories (including a real Windows file lock and rollback); the whole check, download, verify,
  unpack, smoke-test pipeline against a local web server; applying an update to a *running* program and restarting
  it; and the Help menu / Preferences / offer dialog offscreen.

## Known limits

- An update is swapped in when the program exits. If a *second* copy of the program is still running from the
  same folder at that moment, Windows refuses the rename (the helper retries, then leaves the update for the
  next start); on Linux/macOS the swap succeeds, so close other copies before expecting the new version.
- Installs in folders that need administrator rights are not offered updates at all (no "download it yourself" notice yet).
- Failed background checks are quiet and retried; the log has the reason.
