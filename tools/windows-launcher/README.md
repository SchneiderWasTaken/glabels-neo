# gLabels Neo launcher for Windows

A Start menu entry that **keeps gLabels Neo up to date by itself**: every time you open it, it checks
GitHub, downloads a newer build if there is one, and then starts the program.

## Install

```powershell
powershell -ExecutionPolicy Bypass -File .\Install-GlabelsNeoLauncher.ps1
```

No administrator rights are needed. It installs for the current user only, downloads the current build
(about 20 MB), and adds **gLabels Neo** to the Start menu. Open Start and type `gLabels Neo`.

To follow stable releases instead of the newest build of `main`:

```powershell
powershell -ExecutionPolicy Bypass -File .\Install-GlabelsNeoLauncher.ps1 -Channel stable
```

## How it works

| Step | What happens |
|:--|:--|
| Check | Asks GitHub for the current release (`edge` = newest build of `main`, `stable` = latest release) and compares its SHA-256 with the one installed. Takes well under a second. |
| Download | If they differ, downloads the Windows package with BITS (falls back to a plain download) and shows a small progress window. |
| Verify | Checks the SHA-256 against the release's `SHA256SUMS`. If the GitHub CLI (`gh`) is installed and signed in, it also verifies the **build provenance attestation**, proving the package was built by this repository's release workflow. A package that fails either check is thrown away. |
| Install | Unpacks next to the old copy and swaps the folders, so a failed update never leaves a half-installed program. |
| Start | Starts the program, passing along any file you opened (for example a `.glabels` file). |

It never gets in your way:

- **Offline or GitHub down?** The installed version starts and the reason is logged.
- **Already running?** An update is *deferred* until the program is closed; replacing files under a running
  program can make it load mismatched plugins. It starts normally.
- **Update failed?** The working install is left exactly as it was.

## Files

Everything lives in `%LOCALAPPDATA%\gLabelsNeo`:

| File | Purpose |
|:--|:--|
| `app\` | the installed program |
| `installed.json` | which build is installed (channel, name, SHA-256) |
| `config.json` | `channel` (`edge` or `stable`) and `verifyAttestation` (default `true`) |
| `launcher.log` | what the launcher did and why (kept under 200 KB) |

## Useful options

Run the launcher directly for diagnostics:

```powershell
& "$env:LOCALAPPDATA\gLabelsNeo\glabels-neo-launcher.ps1" -CheckOnly      # what would happen
& "$env:LOCALAPPDATA\gLabelsNeo\glabels-neo-launcher.ps1" -ForceUpdate    # reinstall now
& "$env:LOCALAPPDATA\gLabelsNeo\glabels-neo-launcher.ps1" -NoLaunch       # update only
```

## Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File .\Uninstall-GlabelsNeoLauncher.ps1
```

Removes the shortcut and the install folder. Your `.glabels` files and the program's settings are not touched.

## Notes

- The Start menu shortcut runs a tiny `wscript` wrapper (`glabels-neo-launcher.vbs`) so no console window
  flashes up.
- Packages downloaded by the launcher are not marked as "downloaded from the internet", so Windows
  SmartScreen does not prompt. The builds are not code-signed yet.
- The shortcut currently shows the program's built-in icon; the new logo will be used once the icon
  pipeline lands.

## Relationship to the built-in updater

Releases from now on also update themselves from inside the program (see
[Automatic updates](../../docs/AUTO-UPDATE.md)). A copy installed by this launcher is recognised and left to the
launcher, so the two never work on the same folder; the launcher keeps working exactly as described above.
