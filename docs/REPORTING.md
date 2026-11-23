# Reporting a problem (what is sent, and why you can trust it)

gLabels Neo has **Help > Report a Problem...**. This page says exactly what that does.

## What you see and control

- You write a summary and a description.
- You choose what else to include. System details are on by default; the open label and recent program
  messages are **off** until you tick them.
- The dialog shows **exactly what will be sent** before you press anything.
- Nothing is sent until you press **Send report**. Closing the dialog sends nothing.

## What can be included

| Item | Default | Notes |
|:--|:--|:--|
| Summary and description | - | what you typed |
| System details | on | program version, operating system, kernel, CPU architecture, Qt version, display platform, language |
| The open label (`.glabels` XML) | off | it may contain names, addresses and merge data you typed; user and computer names and the home folder are removed from paths |
| Recent program messages | off | the last few hundred log lines kept in memory (never written to disk); user/computer names, e-mail addresses and IPv4 addresses are removed |

Never included: your name, e-mail address, files other than the open label, printer lists, or anything from your
other documents.

## Where it goes

Reports become **public issues** in
[SchneiderWasTaken/glabels-neo](https://github.com/SchneiderWasTaken/glabels-neo/issues). Anyone can read them.
Do not put passwords or private information in a report. If you attach a label that contains private data,
untick that box or remove the data first.

Path of a report:

1. The app asks the **report relay** (`relay/`, a DigitalOcean Function run by the maintainer) for a one-time
   challenge, solves it (about a second of computer time, to deter spam), and sends the report over HTTPS.
2. The relay files the issue on GitHub. The relay stores nothing itself. For rate limiting, the issue it files carries a hidden note with a salted hash of your
   IP address (not the address itself) and the one-time challenge id.
3. Automation reads the issue to label it, look for duplicates and sometimes propose a fix.
   See [AUTOMATION.md](AUTOMATION.md).

If the relay is unavailable, or this build was made without one, **Open on GitHub instead** opens a pre-filled
GitHub issue form in your browser; you review it and submit it with your own GitHub account.

## Turning it off

The feature does nothing on its own. If you do not want it, do not press Send. Builds from source leave the relay
address empty (`-DGLABELS_REPORT_ENDPOINT=`), in which case only the browser option exists.
