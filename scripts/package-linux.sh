#!/bin/bash
# package-linux.sh — build distributable Linux tarball for glabels-neo
#
# Produces a self-contained directory (bin/, lib/, plugins/, share/) with
# Qt libraries and plugins bundled, plus launcher scripts, then tars it up.
# Build on the oldest supported distro for best glibc compatibility.
#
# Usage:  scripts/package-linux.sh [build-dir] [output-tarball]
set -euo pipefail

BUILD_DIR="${1:-build}"
OUT_TARBALL="${2:-glabels-neo-linux-x86_64.tar.gz}"
STAGE="$(mktemp -d)"

trap 'rm -rf "$STAGE"' EXIT

cmake --install "$BUILD_DIR" --prefix "$STAGE/glabels-neo"

QMAKE_BIN="$(command -v qmake6 || command -v qmake)"
QT_LIBS="$( "$QMAKE_BIN" -query QT_INSTALL_LIBS )"
QT_PLUGINS="$( "$QMAKE_BIN" -query QT_INSTALL_PLUGINS )"

PREFIX="$STAGE/glabels-neo"
mkdir -p "$PREFIX/lib" "$PREFIX/plugins"

#--- Qt plugins ------------------------------------------------------------
for p in platforms imageformats iconengines styles tls networkinformation \
         platforminputcontexts xcbglintegrations \
         wayland-decoration-client wayland-graphics-integration-client; do
   if [ -d "$QT_PLUGINS/$p" ]; then
      cp -r "$QT_PLUGINS/$p" "$PREFIX/plugins/"
   fi
done

#--- Shared libraries (fixpoint over ldd) -----------------------------------
is_elf() { file "$1" 2>/dev/null | grep -q 'ELF'; }

copy_lib() {
   local lib="$1"
   local base
   base="$(basename "$lib")"
   if [ ! -f "$PREFIX/lib/$base" ]; then
      cp -L "$lib" "$PREFIX/lib/$base"
      echo "packaged: $base"
      return 0
   fi
   return 1
}

need_lib() {
   # Copy if the library lives outside the baseline system (bundled Qt,
   # /usr/local e.g. zint, or known extras like qrencode)
   local lib="$1"
   case "$lib" in
      "$QT_LIBS"/*|/usr/local/lib/*|*libqrencode*) copy_lib "$lib" && return 0 ;;
   esac
   return 1
}

changed=1
pass=0
while [ "$changed" -eq 1 ] && [ "$pass" -lt 12 ]; do
   pass=$((pass + 1))
   changed=0
   while IFS= read -r -d '' bin; do
      if ! is_elf "$bin"; then continue; fi
      # shellcheck disable=SC2012
      ldd "$bin" 2>/dev/null | awk '/=> \//{print $3} /^\//{print $1}' | while read -r lib; do
         [ -f "$lib" ] || continue
         if need_lib "$lib"; then
            echo "packaged dep of $(basename "$bin"): $(basename "$lib")"
         fi
      done
   done < <(find "$PREFIX" -type f -print0)
   # re-check: if new files appeared this pass, loop again
   newcount="$(find "$PREFIX/lib" -type f | wc -l)"
   if [ "$newcount" -ne "${lastcount:-0}" ]; then
      changed=1
      lastcount="$newcount"
   fi
done

#--- Launchers --------------------------------------------------------------
make_launcher() {
   local name="$1"
   cat > "$PREFIX/$name" <<EOF
#!/bin/bash
DIR="\$(cd "\$(dirname "\$(readlink -f "\$0")")" && pwd)"
export LD_LIBRARY_PATH="\$DIR/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="\$DIR/plugins\${QT_PLUGIN_PATH:+:\$QT_PLUGIN_PATH}"
exec "\$DIR/bin/$2" "\$@"
EOF
   chmod +x "$PREFIX/$name"
}
make_launcher glabels-neo       glabels-qt
make_launcher glabels-fill      glabels-fill
make_launcher glabels-batch-qt  glabels-batch-qt

#--- Tarball ----------------------------------------------------------------
tar -czf "$OUT_TARBALL" -C "$STAGE" glabels-neo
echo "Packaged: $OUT_TARBALL"
