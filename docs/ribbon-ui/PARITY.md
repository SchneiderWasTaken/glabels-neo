# Parity checklist

Copy each bullet group from SPEC.md section 10 here as `- [ ]` items and tick them only when
verified in a running build (note the commit hash). Unticked items at the end of a work
package are reported, not hidden.

## Project and files
- [ ] New
- [ ] Open
- [ ] Open recent (list updates)
- [ ] Save
- [ ] Save as
- [ ] Close with unsaved-changes prompt
- [ ] Exit
- [ ] Title shows "(modified)"
- [ ] Welcome page: New, Browse, Recent
- [ ] Several windows
- [ ] Product Template Designer, every page
- [ ] Preferences: units, grid, appearance, startup, updates

## Editing
- [ ] Undo and redo with descriptive labels
- [ ] Cut, copy, paste, delete
- [ ] Select all, unselect all
- [ ] Six object tools and select mode
- [ ] Order, rotate, flip, align (6), center (3)
- [ ] Canvas context menu (with and without selection)
- [ ] Grid, markup, zoom (4), rulers

## Object properties
- [ ] Text: content, font, size, B/I/U, color, h/v align, line spacing, wrap, auto-shrink
- [ ] Barcode: style menu, show text, checksum, color, data; no shadow
- [ ] Image: file, browse, field, original size, reset
- [ ] Line and fill: width, color, fill color
- [ ] Position, size, lock aspect, line length and angle
- [ ] Shadow: enable, offsets, color, opacity

## Label / Data / Print / Deploy
- [ ] Label tab and page complete
- [ ] Merge: nine formats, location, browse, reload, records, select/unselect all
- [ ] Variables: add, edit, delete, types, increments, step
- [ ] Fill: every button, preview combo, destination, start position
- [ ] Print: range, copies, collate, groups, options, preview, system dialog
- [ ] Deploy: every field, build, results

## App
- [ ] Check for updates, consent, available, progress
- [ ] Report a problem
- [ ] About with License and Website
- [ ] Manual (F1)
- [x] Light, dark, system; Flat, Vector; no restart (354d802, TestThemeGallery::switchesWithoutRestart on the offscreen platform; System checked on Windows 11 light only)
- [ ] Kiosk app and admin dialogs

## Seen rendered, not exercised (Opus, 2026-11-25, offscreen captures in screenshots/)

Not ticked above, because nothing was clicked or edited; listed so the next tester knows what is
already known to draw correctly. Light and dark for each:
Welcome (New, Browse, Recent cards), File backstage nav and Open pane, Home tab with every
group, contextual Text / Barcode / Image / Shape (box and ellipse) / Line tabs appearing for a
single selected object of that type (asserted by TestThemeGallery), Content panel per type,
rulers, grid and markup drawn, Label page and tab, Data > Merge / Variables / Fill pages and
tabs, Print page and tab, Deploy page and tab, View tab, Help tab, status bar text, and every
dialog d01..d11 including the kiosk window and its admin dialog. Differences from the mockups
are listed in VISUAL-REVIEW.md.
