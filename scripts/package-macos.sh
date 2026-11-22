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
bash "$(dirname "$0")/stage-licenses.sh" "$STAGE/dmg" "$BUILD_DIR"

# Applications symlink carries quarantine-ish provenance; drop xattrs from
# everything so codesign artifacts stay valid.
xattr -cr "$STAGE/dmg" 2>/dev/null || true

rm -f "$OUT_DMG"

# hdiutil intermittently fails with "Resource busy" on CI runners (a race with the macOS
# disk services), so retry a few times before giving up.
attempt=1
until hdiutil create \
   -volname "gLabels Neo" \
   -srcfolder "$STAGE/dmg" \
   -ov \
   -format UDZO \
   "$OUT_DMG"
do
   if [ "$attempt" -ge 5 ]; then
      echo "hdiutil create failed after $attempt attempts" >&2
      exit 1
   fi
   echo "hdiutil create failed (attempt $attempt); retrying..." >&2
   attempt=$((attempt + 1))
   sleep $((attempt * 5))
   rm -f "$OUT_DMG"
done

echo "Packaged: $OUT_DMG"
