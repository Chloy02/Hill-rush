#!/bin/bash
# Rebuilds every release package together so they never drift apart:
#   Linux:   AppImage, .deb, .tar.gz   (scripts/build-release.sh)
#   Windows: portable .zip with HillRush.exe (scripts/build-windows.sh)
# and writes one dist/SHA256SUMS covering all of them.
# Follow with scripts/test-packages.sh and scripts/test-windows.sh.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
"$root/scripts/build-release.sh"
"$root/scripts/build-windows.sh"

cd "$root/dist"
find . -maxdepth 1 -type f ! -name SHA256SUMS -printf '%f\n' | sort | xargs sha256sum > SHA256SUMS
echo "== all packages"
cat SHA256SUMS
