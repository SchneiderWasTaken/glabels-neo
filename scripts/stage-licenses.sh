#!/bin/bash
# stage-licenses.sh - copy license and notice files into a package staging directory.
#
# Usage:  scripts/stage-licenses.sh <dest-dir> [build-dir]
#
# Produces in <dest-dir>:
#   LICENSE, THIRD-PARTY-NOTICES.md, CREDITS.md
#   licenses/GPL-3.0.txt, licenses/LGPL-3.0.txt, licenses/templates-MIT.txt
#   licenses/QXlsx-MIT.txt   (when QXlsx was fetched/built by CMake)
#   licenses/qt/             (when the Qt installation ships its LICENSES directory)
set -euo pipefail

DEST="${1:?usage: stage-licenses.sh <dest-dir> [build-dir]}"
BUILD_DIR="${2:-build}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mkdir -p "$DEST/licenses"

cp "$ROOT/LICENSE"                 "$DEST/LICENSE"
cp "$ROOT/THIRD-PARTY-NOTICES.md"  "$DEST/THIRD-PARTY-NOTICES.md"
cp "$ROOT/CREDITS.md"              "$DEST/CREDITS.md"
cp "$ROOT/LICENSE"                 "$DEST/licenses/GPL-3.0.txt"
cp "$ROOT/glbarcode/LICENSE"       "$DEST/licenses/LGPL-3.0.txt"
cp "$ROOT/templates/LICENSE"       "$DEST/licenses/templates-MIT.txt"

# QXlsx is fetched at configure time when no system package is available.
for candidate in "$BUILD_DIR"/_deps/qxlsx-src/LICENSE*; do
   if [ -f "$candidate" ]; then
      cp "$candidate" "$DEST/licenses/QXlsx-MIT.txt"
      break
   fi
done

# Qt license texts, when present in the Qt installation.
QMAKE_BIN="$(command -v qmake6 || command -v qmake || true)"
if [ -n "$QMAKE_BIN" ]; then
   QT_PREFIX="$("$QMAKE_BIN" -query QT_INSTALL_PREFIX)"
   if [ -d "$QT_PREFIX/LICENSES" ]; then
      mkdir -p "$DEST/licenses/qt"
      cp -R "$QT_PREFIX/LICENSES/." "$DEST/licenses/qt/"
   else
      echo "warning: no Qt LICENSES directory under $QT_PREFIX" >&2
   fi
fi

echo "Staged license files in $DEST"
