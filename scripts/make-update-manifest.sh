#!/bin/bash
# make-update-manifest.sh - describe a release for the in-app updater.
#
# The application checks for new versions by downloading one tiny JSON file from a fixed URL:
#
#   stable:  https://github.com/<repo>/releases/latest/download/glabels-neo-update-manifest.json
#   edge:    https://github.com/<repo>/releases/download/edge/glabels-neo-update-manifest.json
#
# That is a plain static download: no GitHub API, no rate limit, no token, and the file is ours
# (versioned schema) rather than GitHub's.  It names the release, the commit it was built from and,
# per platform, the package with its size and SHA-256; the application verifies both after download.
#
# Usage:  scripts/make-update-manifest.sh DIST_DIR CHANNEL TAG COMMIT REPO [COMMIT_TIME]
#
#   DIST_DIR  directory holding the release packages (glabels-neo-*)
#   CHANNEL   stable | edge
#   TAG       tag the packages are attached to: "v1.3.0" for stable, "edge" for edge
#   COMMIT    full hash of the commit the packages were built from
#   REPO      GitHub repository, owner/name
#   COMMIT_TIME  committer time of COMMIT, seconds since the epoch; default: asked of git (needs COMMIT
#             in the current repository).  Clients order edge builds by it, so they never move backwards.
#
# Writes DIST_DIR/glabels-neo-update-manifest.json.  Needs jq.
# The schema is read by glabels/update/UpdateManifest.cpp; keep the two in step.
set -euo pipefail

if [ "$#" -lt 5 ] || [ "$#" -gt 6 ]; then
   echo "usage: $0 DIST_DIR CHANNEL TAG COMMIT REPO [COMMIT_TIME]" >&2
   exit 2
fi

dist="$1"
channel="$2"
tag="$3"
commit="$4"
repo="$5"
commit_time="${6:-}"

case "$channel" in
   stable|edge) ;;
   *) echo "channel must be 'stable' or 'edge', not '$channel'" >&2; exit 2 ;;
esac

if ! [[ "$commit" =~ ^[0-9a-f]{40}$ ]]; then
   echo "commit must be a full 40-character hash: '$commit'" >&2
   exit 2
fi

if [ -z "$commit_time" ]; then
   commit_time="$(git show -s --format=%ct "$commit" 2>/dev/null || true)"
fi
if ! [[ "$commit_time" =~ ^[0-9]+$ ]]; then
   echo "cannot determine the commit time of $commit (pass it as the 6th argument)" >&2
   exit 2
fi

sha256_of() {
   if command -v sha256sum >/dev/null 2>&1; then
      sha256sum "$1" | cut -d' ' -f1
   else
      shasum -a 256 "$1" | cut -d' ' -f1
   fi
}

# platform key (as the application names its platform) -> package file name
packages=(
   "windows-x86_64:glabels-neo-windows-x64.zip"
   "linux-x86_64:glabels-neo-linux-x86_64.tar.gz"
   "macos-arm64:glabels-neo-macos-arm64.dmg"
)

assets='{}'
count=0
for entry in "${packages[@]}"; do
   key="${entry%%:*}"
   name="${entry#*:}"
   file="$dist/$name"

   if [ ! -f "$file" ]; then
      echo "note: no package for $key ($name); it will not be offered for updates" >&2
      continue
   fi

   size="$(wc -c < "$file" | tr -d '[:space:]')"
   assets="$(jq -c \
      --arg key "$key" \
      --arg name "$name" \
      --arg url "https://github.com/$repo/releases/download/$tag/$name" \
      --arg sha "$(sha256_of "$file")" \
      --argjson size "$size" \
      '. + {($key): {name: $name, url: $url, sha256: $sha, size: $size}}' <<< "$assets")"
   count=$((count + 1))
done

if [ "$count" -eq 0 ]; then
   echo "no release packages found in $dist" >&2
   exit 1
fi

jq -n \
   --arg channel "$channel" \
   --arg version "$tag" \
   --arg commit "$commit" \
   --argjson commit_time "$commit_time" \
   --arg published "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
   --arg notes "https://github.com/$repo/releases/tag/$tag" \
   --argjson assets "$assets" \
   '{schema: 1, channel: $channel, version: $version, commit: $commit, commit_time: $commit_time,
     published: $published, notes_url: $notes, assets: $assets}' \
   > "$dist/glabels-neo-update-manifest.json"

echo "wrote $dist/glabels-neo-update-manifest.json ($count package(s))"
