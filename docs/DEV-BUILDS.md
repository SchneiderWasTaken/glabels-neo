# Dev builds

The `dev` branch is where changes are put together and tried before they reach `main`. Every push to
`dev` builds, tests and packages gLabels Neo for Windows, macOS and Linux, exactly like a push to
`main`, and publishes the result as one rolling prerelease named **`dev`**:

    https://github.com/SchneiderWasTaken/glabels-neo/releases/tag/dev

| | `main` (edge) | `dev` |
|---|---|---|
| Release | rolling prerelease `edge` | rolling prerelease `dev` |
| Title bar | gLabels Neo | gLabels Neo **(dev)** |
| About | edge `<commit>` | **dev `<commit>`**, a "Development build" notice |
| Self-update | yes, if the person opted in | **never** (no update manifest is published) |
| Used for | what the next stable release is cut from | trying a change, or several together |

A dev build cannot update itself, and an installed edge or stable copy never offers to update to a
dev build, so the two cannot get mixed up. Install a newer dev build by downloading it from the
release page. Bug reports from a dev build carry `Build flavor: dev` in their environment block.

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
`dev`, and the `publish` job has a "Publish dev release" step. A manual run on `dev` with
`publish = dev` publishes too; with `dry-run` it only builds.
