#!/bin/bash
# check-license-headers.sh - repository hygiene checks run in CI (lint workflow).
#
#  1. Every first-party C/C++ source file carries a GPL or LGPL header.
#  2. No template placeholders (e.g. "[Your Name]") remain in tracked files.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
fail=0

while IFS= read -r f; do
   case "$f" in
      # Tiny compatibility stubs for private headers of the MIT-licensed QXlsx library.
      glabels/qxlsx_compat/*) continue ;;
   esac
   if ! grep -q -E 'GNU (Lesser )?General Public License' "$f"; then
      echo "missing license header: $f"
      fail=1
   fi
done < <(git ls-files '*.cpp' '*.hpp' '*.h' '*.c')

if git grep -n -I -E '\[Your Name\]|your-username|<your-org>' -- . ':!scripts/check-license-headers.sh'; then
   echo "error: template placeholder text found (see matches above)"
   fail=1
fi

if [ "$fail" -ne 0 ]; then
   exit 1
fi
echo "license headers and placeholders OK"
