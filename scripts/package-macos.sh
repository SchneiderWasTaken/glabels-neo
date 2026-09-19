#!/bin/bash
# package-macos.sh — build distributable macOS DMG for glabels-neo
#
# Stages the installed app bundles into a drag-to-Applications DMG.
# Expects the CMake build to have been configured and built already.
#
# Usage:  scripts/package-macos.sh [build-dir] [output-dmg]
set -euo pipefail

BUILD_DIR="${1:-build}"
OUT_DMG="${2:-glabels-neo-macos.dmg}"
STAGE="$(mktemp -d)"

trap 'rm -rf "$STAGE"' EXIT

# Install the configured build into the staging prefix (this also runs the
# Qt deploy script + MacDeployFixup + ad-hoc codesign).
cmake --install "$BUILD_DIR" --prefix "$STAGE/app"

mkdir -p "$STAGE/dmg"
cp -R "$STAGE/app/glabels-qt.app" "$STAGE/dmg/"
cp -R "$STAGE/app/glabels-fill.app" "$STAGE/dmg/"
ln -s /Applications "$STAGE/dmg/Applications"

# Applications symlink carries quarantine-ish provenance; drop xattrs from
# everything so codesign artifacts stay valid.
xattr -cr "$STAGE/dmg" 2>/dev/null || true

rm -f "$OUT_DMG"
hdiutil create \
   -volname "gLabels" \
   -srcfolder "$STAGE/dmg" \
   -ov \
   -format UDZO \
   "$OUT_DMG"

echo "Packaged: $OUT_DMG"
