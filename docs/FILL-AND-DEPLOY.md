# Fill and Deploy Guide

This document covers the three major features gLabels-neo adds to the
original glabels-qt: the **Fill page**, the **Deploy tab**, and the
**kiosk build**.


## Fill Page

The Fill page (left TOC → "Fill", or Ctrl+5) is an interactive batch
table for filling and printing labels without leaving a print-merge
workflow.

### Layout

| Column | Purpose |
|:-------|:--------|
| **Qty** | How many copies of this label to print |
| **Print** | Checkbox — uncheck to stage a row without printing it |
| **Field columns** | One per `${FieldName}` in your label text |

A **Preview** combo above the table controls what the live sheet preview
on the right shows:

- **Selected row** — only the row you clicked (focused editing)
- **Checked rows (what will print)** — default; WYSIWYG with the Print button
- **All rows** — every row regardless of Print flag

### CSV / Excel Import / Export

- **Import CSV / Import Excel** — loads rows from a file into the table.
  The first row must be a header: `Qty,Print,Field1,Field2,...`
- **Export CSV / Export Excel** — writes the current table to a file.

CSV example:
```
Qty,Print,Name,Price
3,1,Apples,0.99
2,1,Bananas,0.59
5,0,Cherries,3.99
```
(Row 3 has Print=0, so it's staged but won't print until re-checked.)

### Print

The **Print** button sends the **checked** rows to the printer (or PDF),
starting at the position you set.  The preview mode is independent — you
can preview one row while printing all checked rows.


## Deploy Tab

The Deploy tab (left TOC → "Deploy") lets you build a standalone kiosk
executable from the current project.

### Configuration

| Setting | Description |
|:--------|:------------|
| **Application name** | Shown in the kiosk's title bar |
| **Admin PIN** | Locks the Ctrl+Shift+A admin mode (blank = no PIN) |
| **Printer** | Locked printer for the kiosk (blank = system default) |
| **Data source** | Excel/CSV file whose rows pre-fill the Fill table |
| **Embedded label name** | Display name for the baked-in label |
| **Locked fields** | Checkboxes — checked = read-only to the employee |
| **Base kiosk executable** | Path to the SFX base (auto-detected) |

### Building

1. Open your label project in the designer.
2. Go to the **Deploy** tab.
3. Set the application name, admin PIN, and printer as desired.
4. If using a data source, browse to an Excel/CSV file and check the
   fields that should be pre-filled (locked).
5. Click **Build kiosk…** and choose an output path.

The result is a single `.exe` file that:
- Embeds your label layout (no external file needed)
- Embeds the template database (no templates folder needed)
- Contains all Qt/runtime DLLs (self-extracting to `%TEMP%`)
- Reads its configuration from a trailer appended to itself

### Testing the kiosk

Double-click the built `.exe`.  It should:
- Open with your label's fields as columns in the Fill table
- If a data source was configured, pre-fill the locked columns
- Show a live preview
- Print to the configured printer (or system default)

Press **Ctrl+Shift+A** to open the admin dialog (enter PIN if set).
You can change the label, printer, PIN, and branding, then save — the
kiosk rewrites its own config trailer.


## Kiosk Mode (glabels-fill)

The kiosk is a separate executable target (`glabels-fill`) that shares
the `glabels_lib` static library with the full designer but excludes
all editing UI.

### What's included

- Fill table (Qty, Print, field columns)
- Live sheet preview
- CSV import (Excel is omitted in the static single-file build)
- Print button + system print dialog
- Admin mode (Ctrl+Shift+A → PIN → reconfigure)

### What's NOT included

- Label editor / object creation
- Template designer
- Variables page
- Merge page
- Properties page

### Single-file SFX packaging

The kiosk is built with **static Qt** (no Qt DLLs) and the template
database is embedded as a Qt resource.  A small C launcher stub (the
SFX) wraps the static exe + ~30 dependency DLLs.  At runtime it:

1. Extracts the payload to `%TEMP%\glabels-fill-<pid>\`
2. Sets `GLABELS_FILL_SFX_PATH` so the inner exe can find its config
3. Launches the inner exe and waits
4. Cleans up the temp directory on exit

The result is one ~44 MB `.exe` that runs on any Windows 10+ machine
with no installation, no DLLs, and no external files.


## Data-Source Model

The "locked vs. editable" column model supports the retail use case
where some data comes from a spreadsheet and the employee enters the
rest:

1. **Setup** (in the Deploy tab): browse to an Excel/CSV file as the
   data source.  Check the fields that should be pre-filled from it
   (locked).  Leave other fields unchecked (employee-entered).

2. **At the terminal** (kiosk): the locked columns are pre-filled from
   the source file and rendered read-only.  The employee only types
   into the open columns, sets Qty, checks Print, and hits Print.

Example: an Excel file has `Name,Description` columns.  The label has
`${Name}\n${Description}\n${Price}`.  Lock `Name` and `Description`;
leave `Price` open.  The employee sees pre-filled names/descriptions
and types today's price for each row.


## Portable Build

The full designer can also be shipped as a no-install portable folder.
See `scripts/build-portable.ps1` (coming soon) or build manually:

1. Build the dynamic `glabels-qt` target.
2. Run `windeployqt` on the exe.
3. Copy the `templates/` folder next to the exe.
4. Copy the runtime DLLs (see `scripts/bundle-deps.ps1`).
