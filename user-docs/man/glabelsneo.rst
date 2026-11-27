glabelsneo
==========

SYNOPSIS
--------

**glabelsneo** *command* [*options*] [*arguments*]

**glabelsneo print** [*options*] [*FILE*]

**glabelsneo inspect** [*options*] *FILE*

**glabelsneo printers** [*options*]

DESCRIPTION
-----------

:program:`glabelsneo` prints gLabels label files from the command line, so that labels can be
produced from scripts, services and other programs as well as by hand.  It works without
prompting (for scripts), with prompts (for a person at a terminal), and in batches described by
JSON.  It supersedes :manpage:`glabels-batch-qt(1)`.

Results go to standard output and diagnostics to standard error.  With ``--format json`` standard
output carries exactly one JSON object describing the run, which is the interface intended for
other programs.

COMMANDS
--------

.. program:: glabelsneo

**print**
    Print a label file on a printer, or to a PDF or PNG files.  ``--printer`` or ``--output`` is
    required.

**inspect**
    Show the template, label size, merge source, user variables, and the fields a label file
    expects to be filled in.

**printers**
    List the installed printers.  The default printer is marked.  ``--check NAME`` tests whether a
    name resolves.

**version**
    Show the version (``--verbose`` for build and system details).

**help** [*command*]
    Show help.

PRINT
-----

Print one label by naming each field the label uses::

    glabelsneo print jeans.glabels --printer default --field price=29.99 --field "name=Blue Jeans"

Values containing spaces need ordinary shell quoting.  ``--printer default`` is the system default
printer; any other name must match an installed printer exactly (or, failing that,
case-insensitively and uniquely).  A name is never guessed from a prefix.

.. option:: -p <name>, --printer <name>

   Print on this printer.  Either ``--printer`` or ``--output`` is required.  Zebra printers are
   driven with ZPL automatically, exactly as the graphical program does; see ``--driver``.

.. option:: -o <file>, --output <file>

   Write a PDF instead of printing.  ``-`` writes the PDF to standard output.  If the name ends in
   ``.png`` the pages are written as PNG images instead (``label.png`` for one page, ``label-1.png``,
   ``label-2.png`` and so on for several), which is a quick way to see what a job will look like.

.. option:: -F <NAME=VALUE>, --field <NAME=VALUE>

   Set the merge field *NAME* for the label.  May be repeated.  The value is everything after the
   first ``=``.  Formatting and default modifiers in the label (``${price:%.2f}``,
   ``${note:=none}``) work as usual.

.. option:: --var <NAME=VALUE>

   Set the initial value of a user variable (serial numbers and the like).  If the label also has a
   merge field of the same name, the field wins.

.. option:: --data <source>

   Records for a batch: one label per record.  A file name, or ``-`` for standard input.

.. option:: --data-format <format>

   The format of ``--data``: ``json``, ``jsonl``, ``csv``, ``tsv``, ``colon`` or ``semicolon``.  The
   text formats read field names from the first line; add ``-nokeys`` (for example ``csv-nokeys``)
   for files without a header, whose fields are then named ``1``, ``2``, and so on.  By default the
   format is chosen from the file name (``.json``, ``.jsonl``, ``.ndjson``, ``.csv``, ``.tsv``);
   it is required for ``--data -``.

.. option:: --job <file>

   Run a JSON job document (see below).  ``-`` reads it from standard input.  Options given on the
   command line override the document: a ``--printer`` or ``--output`` replaces the destination
   of every job, ``--field`` and ``--var`` are added, and ``--data`` replaces the inline labels.

.. option:: -n <n>, --copies <n>

   Copies of each label.  Default 1.

.. option:: --sheets <n>

   Full sheets of a label that has no fields or data.

.. option:: --first <n>

   Start at position *n* on the first sheet.  Default 1.

.. option:: --collate

   Collate copies of merged labels (A B A B instead of A A B B).

.. option:: --group

   Start each merge group on a new page.

.. option:: --outlines, --crop-marks, --reverse

   Print label outlines, crop marks, or a mirror image.

.. option:: -i, --interactive

   Prompt for every field (the current value is shown in brackets; Enter keeps it) and ask for
   confirmation before sending.  Afterwards it offers to print another label with different values.

.. option:: -y, --yes

   Do not ask for confirmation.

.. option:: --dry-run

   Check everything, report what would be printed, and print nothing.

.. option:: --force-fields

   Make ``--field`` values override the data file's values.  Without it the data file wins and
   ``--field`` supplies defaults for records that lack the field.

.. option:: --strict

   Treat warnings (below) as errors.

.. option:: --max-labels <n>

   Refuse to print more than *n* labels, to catch runaway batches before they use up label stock.
   Default 1000; ``0`` removes the limit.  At a terminal it asks instead of refusing.

.. option:: --continue-on-error

   In a run of several jobs, keep going after a job fails to print.  Without it the run stops at the
   first failure.  Problems found while checking the jobs always stop the run before anything is sent.

.. option:: --driver <auto|system|zpl>

   How to talk to the printer.  ``auto`` (the default) uses ZPL for printers recognised as Zebra
   printers and the operating system's driver for everything else.  Use ``system`` or ``zpl`` to
   override a wrong guess.

.. option:: --dpi <n>

   Resolution of PNG output.  Default 150.

Global options (accepted by every command)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. option:: --format <text|json>

   Output format on standard output.  Default ``text``.

.. option:: -q, --quiet

   Errors only.

.. option:: -v, --verbose

   Show library diagnostics on standard error.

.. option:: --no-input

   Never prompt, even at a terminal.  The environment variable ``GLABELSNEO_NO_INPUT=1`` does the
   same.  Prompts only ever happen when standard input is a terminal.

FIELDS AND WARNINGS
-------------------

Fields a label uses but nobody supplies print blank, and values supplied for fields the label does
not use are ignored; neither is an error.  Both produce a warning (on standard error, or in the
``warnings`` list of the JSON result) so that a typo does not go unnoticed::

    warning: field 'prise' is not used by the label; did you mean 'price'?
    warning: the label uses field 'price' but no value was supplied; it will print blank

Use ``glabelsneo inspect`` to see which fields a file expects, and ``--strict`` to make warnings
fail the run.

BATCHES
-------

JSON records (``--data records.json``) are an array of flat objects, or JSON Lines with one object per
line::

    [ {"name": "Blue Jeans", "price": 29.99},
      {"name": "Corduroy Pants", "price": 49.5, "_copies": 2} ]

Values become text: numbers in their shortest exact form (``29.99``), booleans as ``true`` or
``false``, ``null`` as nothing.  Numbers are exact up to 2 to the 53rd power, so send longer
identifiers (tracking numbers, barcodes) as strings.  Nested objects and arrays are an error.  A key
starting with an underscore is reserved: ``_copies`` asks for that many copies of the record.  The
input must be UTF-8 (Windows PowerShell 5.1 writes UTF-16 for ``>``; use ``Out-File -Encoding utf8``).

A job document describes whole runs, including which file, which printer, and the data, so another
program needs nothing else::

    {
      "version": 1,
      "jobs": [
        { "id": "order-1041-line-1",
          "file": "templates/jeans.glabels",
          "printer": "Brother QL-800",
          "options": { "copies": 1, "first": 1, "collate": false },
          "variables": { "batch": "A7" },
          "defaults": { "currency": "USD" },
          "labels": [ { "fields": { "price": "29.99", "name": "Blue Jeans" }, "copies": 2 },
                      { "fields": { "price": "49.50", "name": "Corduroy Pants" } } ] },
        { "id": "order-1041-shipping",
          "file": "templates/ship.glabels",
          "output": "out/ship-1041.pdf",
          "data": "ship-1041.jsonl" }
      ]
    }

A document with ``labels`` or ``data`` at the top level and no ``jobs`` describes a single job.
Relative paths in a document are resolved against the document's own directory.  Each job needs
exactly one of ``printer`` or ``output`` (they may come from the command line instead).  Unknown
keys are errors.  Within a label a field's value is, from lowest to highest priority, the job's
``defaults``, then ``--field``, then the label's own ``fields`` (``--force-fields`` puts ``--field``
last).  ``id`` is copied to the results so callers can match them up.

Every job is checked before anything is sent: if any job has a bad file, bad data or an unknown
printer, nothing is printed at all.

RESULT FORMAT
-------------

With ``--format json`` standard output is one object::

    { "ok": true, "dryRun": false, "exitCode": 0,
      "warnings": [ { "code": "field.unused", "message": "...", "job": "..." } ],
      "jobs": [ { "id": "...", "index": 0, "status": "printed", "file": "...",
                  "printer": "Brother QL-800", "labels": 3, "pages": 3 } ],
      "errors": [] }

``status`` is ``printed``, ``pdf``, ``image``, ``checked`` (dry run), ``failed`` or ``skipped``.
Codes are stable and part of the interface; messages are for people and may change.  Warning codes
are ``field.missing``, ``field.unused``, ``option.ignored`` and ``printer.fallback``.  Error codes
include ``usage``, ``input.missing``, ``input.invalid``, ``data.invalid``, ``data.missing``,
``data.empty``, ``data.limit``, ``data.strict``, ``printer.notfound``, ``printer.ambiguous``,
``printer.failed`` and ``cancelled``.

EXIT STATUS
-----------

0
    Success: the job was handed to the printer system, or the file was written.  With
    ``--dry-run``, all checks passed.  This does not mean paper has come out.

1
    Unexpected internal error.

2
    Usage error: bad, unknown or conflicting options, or no ``--printer``/``--output``.

3
    The label file was not found, is unreadable, or is not a gLabels file.

4
    Data error: malformed JSON or records, over the ``--max-labels`` limit, or warnings under
    ``--strict``.

5
    Printer error: not found, ambiguous, or the job could not be sent.

6
    Some jobs printed and some failed (``--continue-on-error``).

130
    Cancelled at a prompt.

EXAMPLES
--------

Print one label, naming each field::

    glabelsneo print price-tag.glabels -p default -F price=9.99 -F "name=Blue Jeans"

Check first, then print a batch from a spreadsheet export::

    glabelsneo print tags.glabels -p "Brother QL-800" --data items.csv --dry-run
    glabelsneo print tags.glabels -p "Brother QL-800" --data items.csv

Be asked for the values::

    glabelsneo print price-tag.glabels -p default -i

Pipe records from another program, and read the result::

    producer | glabelsneo print tags.glabels -p default --data - --data-format jsonl --format json

Look at a job without a printer::

    glabelsneo print price-tag.glabels -F price=9.99 -F name=Jeans -o preview.png

FILES
-----

The command line tool shares its settings, user templates and thermal printer connection profiles
with the graphical program.

KNOWN ISSUES
------------

``glabelsneo`` prints through the operating system, so success means the job was spooled.  It cannot
tell whether the printer was out of labels.

The macOS disk image does not yet include the command line tool.

SEE ALSO
--------

:manpage:`glabels-qt(1)`, :manpage:`glabels-batch-qt(1)`

The gLabels Neo homepage at <https://github.com/SchneiderWasTaken/glabels-neo>.
