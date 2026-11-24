#!/bin/bash
# Cross-compiles the Windows build in a container and writes
# dist/HillRush-<version>-windows-x64.zip (a single self-contained HillRush.exe).
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
version=$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)$/\1/p' "$root/CMakeLists.txt" | head -1)
image=hillrush-win-build:1
stage="$root/build-windows"
dist="$root/dist"
name="HillRush-$version-windows-x64"

rm -rf "$stage"
mkdir -p "$stage/out/$name" "$dist"
umask 022

docker build -q -t "$image" "$root/packaging/windows" >/dev/null
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$root":/src:ro -v "$stage/out":/out "$image" bash -c "
        set -e
        cmake -S /src -B /tmp/build -G Ninja \
            -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-mingw-w64.cmake \
            -DCMAKE_BUILD_TYPE=Release
        cmake --build /tmp/build -j\$(nproc)
        x86_64-w64-mingw32-strip -o /out/$name/HillRush.exe /tmp/build/bin/HillRush.exe
        cp /tmp/build/bin/hillrush_tests.exe /out/
        echo '== DLLs the exe needs:'
        x86_64-w64-mingw32-objdump -p /out/$name/HillRush.exe | grep 'DLL Name' | sort
    "

{
    echo "Hill Rush bundles the following third-party software."
    echo
    echo "=== raylib (zlib/libpng license) ==="; cat "$root/third_party/raylib/LICENSE"; echo
    echo "=== Box2D (MIT license) ==="; cat "$root/third_party/box2d/LICENSE"; echo
    echo "=== ENet (MIT license) ==="; cat "$root/third_party/enet/LICENSE"; echo
    echo "=== Lilita One font (SIL Open Font License 1.1) ==="; cat "$root/assets/fonts/OFL.txt"
} | sed 's/$/\r/' > "$stage/out/$name/THIRD-PARTY-LICENSES.txt"
sed 's/$/\r/' "$root/packaging/windows/README-windows.txt" > "$stage/out/$name/README.txt"

rm -f "$dist/$name.zip"
(cd "$stage/out" && python3 -c "
import sys, zipfile, os
with zipfile.ZipFile(sys.argv[1], 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(os.listdir(sys.argv[2])):
        z.write(os.path.join(sys.argv[2], f), arcname=os.path.join(sys.argv[2], f))
" "$dist/$name.zip" "$name")
echo "== done"
ls -la "$dist/$name.zip" "$stage/out/$name"
