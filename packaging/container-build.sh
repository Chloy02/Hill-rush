#!/bin/bash
# Runs inside the packaging/Dockerfile image. Builds, tests and stages the game.
#   /src  - project sources (read-only)
#   /out  - staging directory on the host
set -euo pipefail

BUILD=/tmp/build
cmake -S /src -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DHILLRUSH_WERROR=ON \
    -DOpenGL_GL_PREFERENCE=LEGACY \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,--as-needed"
cmake --build "$BUILD" -j"$(nproc)"

echo "== unit tests"
"$BUILD/bin/hillrush_tests"

mkdir -p /out/icons
strip -o /out/hillrush "$BUILD/bin/hillrush"
for size in 32 48 64 128 256 512; do
    /out/hillrush --export-icon "/out/icons/${size}.png" --icon-size "$size"
done

echo "== runtime dependencies"
objdump -p /out/hillrush | grep NEEDED
maxglibc=$(objdump -T /out/hillrush | grep -o 'GLIBC_2\.[0-9]*' | sort -uV | tail -1)
echo "highest glibc symbol: $maxglibc"
if [ "$(printf '%s\n' "$maxglibc" GLIBC_2.28 | sort -V | tail -1)" != "GLIBC_2.28" ]; then
    echo "error: binary needs a glibc newer than 2.28" >&2
    exit 1
fi
if objdump -T /out/hillrush | grep -q GLIBCXX; then
    echo "error: binary depends on the system libstdc++" >&2
    exit 1
fi
