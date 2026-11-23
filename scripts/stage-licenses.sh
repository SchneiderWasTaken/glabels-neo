#!/bin/bash
# stage-licenses.sh - copy license and notice files into a package staging directory.
#
# Usage:  scripts/stage-licenses.sh <dest-dir> [build-dir]
#
# Produces in <dest-dir>:
#   LICENSE, THIRD-PARTY-NOTICES.md, CREDITS.md
#   licenses/GPL-3.0.txt, licenses/LGPL-3.0.txt, licenses/templates-MIT.txt
#   licenses/QXlsx-MIT.txt   (when QXlsx was fetched/built by CMake)
#   licenses/*.txt           (libzint, libqrencode, zlib, ICU: third-party-licenses/)
#   licenses/qt/             (Qt's own third-party license texts: third-party-licenses/qt/, overlaid with the
#                             Qt installation's LICENSES directory when it ships one)
#   VERSIONS.txt             (versions of the bundled libraries, best effort)
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

cp "$ROOT"/third-party-licenses/*.txt "$DEST/licenses/"
mkdir -p "$DEST/licenses/qt"
cp "$ROOT"/third-party-licenses/qt/*.txt "$DEST/licenses/qt/"

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
   # qmake.exe prints CRLF line endings on Windows; strip the carriage return.
   QT_PREFIX="$("$QMAKE_BIN" -query QT_INSTALL_PREFIX | tr -d '\r')"
   if [ -d "$QT_PREFIX/LICENSES" ]; then
      mkdir -p "$DEST/licenses/qt"
      cp -R "$QT_PREFIX/LICENSES/." "$DEST/licenses/qt/"
   else
      echo "warning: no Qt LICENSES directory under $QT_PREFIX" >&2
   fi
fi

# Versions of what is bundled (best effort: the package tools differ per platform).
{
   echo "gLabels Neo bundled library versions"
   if [ -n "$QMAKE_BIN" ]; then echo "Qt:           $("$QMAKE_BIN" -query QT_VERSION | tr -d '')"; fi
   for pc in zint libqrencode zlib; do
      v="$(pkg-config --modversion "$pc" 2>/dev/null || true)"
      echo "$pc: ${v:-see the platform package manager}"
   done
   if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
      grep -E '^(LIBZINT|LIBQRENCODE|QXlsx)[A-Za-z_]*VERSION[A-Za-z_]*:' "$BUILD_DIR/CMakeCache.txt" | sed 's/:[A-Z]*=/: /' || true
   fi
} > "$DEST/VERSIONS.txt"

echo "Staged license files in $DEST"
