# Release-train trigger (Windows)

GitHub's scheduler is best-effort and may skip scheduled runs for long stretches. The release train
(`.github/workflows/release-train.yml`) therefore also accepts a plain `workflow_dispatch`, and this small helper
provides a dependable clock: a per-user Windows scheduled task that, every 10 minutes **inside** the two Central
Time release windows (10:00-11:30 and 14:00-15:30), runs

```
gh workflow run release-train.yml -f dry_run=false -f force=false
```

Everything that decides whether to release lives in the workflow (`.github/agents/release-train.mjs`): a slot can be used
once, at most two releases a day, and nothing is released unless something user-visible changed. Extra triggers are
therefore harmless: they are recorded and decline.

Needs the GitHub CLI (`gh`) signed in with permission to run workflows on the repository. Runs only while the user is
logged on. No administrator rights.

```powershell
powershell -ExecutionPolicy Bypass -File .\Install-ReleaseTrainTrigger.ps1      # install / update
powershell -ExecutionPolicy Bypass -File .\Uninstall-ReleaseTrainTrigger.ps1    # remove
```

The task is called `gLabels Neo release train trigger`; its log is `%LOCALAPPDATA%\gLabelsNeo\release-train-trigger.log`.
