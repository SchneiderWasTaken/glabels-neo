#!/bin/bash
# report-missing-libs.sh - list the shared libraries a Linux package needs from the system.
#
# Run it inside a bare distribution image with the unpacked package mounted at /app, to learn
# which system libraries a user's machine must provide (everything the bundle does not carry):
#
#   docker run --rm -v "$PWD/smoke/glabels-neo:/app:ro" \
#      -v "$PWD/scripts/report-missing-libs.sh:/report.sh:ro" ubuntu:22.04 bash /report.sh
#
# Output: one line per missing library with the number of files that need it.
set -euo pipefail

cd /app
find bin lib plugins -type f | while read -r f; do
   LD_LIBRARY_PATH=/app/lib ldd "$f" 2>/dev/null | grep 'not found' || true
done | awk '{print $1}' | sort | uniq -c | sort -rn
