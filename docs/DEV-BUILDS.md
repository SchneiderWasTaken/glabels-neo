# Dev builds

The `dev` branch is where changes are put together and tried before they reach `main`. Every push to
`dev` builds, tests and packages gLabels Neo for Windows, macOS and Linux on GitHub, exactly like a push
to `main` (so the build gets the Google sign-in credentials from the repository's secrets), but the
result is **not published**: there is no release and no download page.

The packages are kept as artifacts of the workflow run for 7 days. Open the **Release** run for the
push on the Actions tab; the run summary says which commit it is, and the **Artifacts** section at the
bottom lists `pkg-windows`, `pkg-macos` and `pkg-linux`.

| | `main` (edge) | `dev` |
|---|---|---|
| Built on GitHub, with the Google secrets | yes | yes |
| Published as a release | rolling prerelease `edge` | **no** |
| Where the package is | release page (public) | run artifacts, 7 days |
| Title bar | gLabels Neo | gLabels Neo **(dev)** |
| About | edge `<commit>` | **dev `<commit>`**, a "Development build" notice |
| Self-update | yes, if the person opted in | **never** (no update manifest) |
| Used for | what the next stable release is cut from | trying a change, or several together |

A dev build cannot update itself, and an installed edge or stable copy never offers to update to a dev
build, so the two cannot get mixed up. Bug reports from a dev build carry `Build flavor: dev` in their
environment block.

**Who can download the artifacts.** This repository is public, and GitHub lets any signed-in GitHub
account download the artifacts of a public repository's runs. So the artifacts are not a private
channel: they are only not *advertised* (no release page, no link in the app). The packages contain the
same Google client credentials as an edge build. If dev builds must be limited to the people who work
on the repository, encrypt them in the workflow with a repository secret, or make the repository
private.

## Putting a change on dev

`dev` is an integration branch: merge the branches you want to try into it and push.

```
git switch dev
git merge --no-ff feat/my-change
git push origin dev
```

Keep `dev` close to `main`: merge `main` into it from time to time. A change reaches users only
through a pull request to `main`, never from `dev`.

## What marks a build as dev

The CMake variable `BUILD_FLAVOR` is `dev` when

* the build runs on GitHub Actions for a push (or manual run) on the `dev` branch, or
* `-DGLABELS_BUILD_FLAVOR=dev` is given, or the environment variable `GLABELS_BUILD_FLAVOR=dev` is set
  (handy for a local build you want to tell apart from a release).

Anything else is `release`. In code, `model::Version::IS_DEV_BUILD` and `model::Version::FLAVOR` carry
the answer; the title bar (`MainWindow::setTitle`) and About (`AboutDialog`) use them.

The workflow side is in `.github/workflows/release.yml`: the `channel` job resolves `dev` for a push to
`dev`; the `publish` job is skipped for it, and a small `dev-summary` job writes the run summary.
