#!/bin/bash
# Builds the release packages:
#   dist/HillRush-<version>-x86_64.AppImage   runs anywhere, no install needed
#   dist/hillrush_<version>_amd64.deb          Debian / Ubuntu / Mint / Pop!_OS
#   dist/hillrush-<version>-linux-x86_64.tar.gz  any distro, with install.sh
# Compilation happens in a glibc 2.28 container (packaging/Dockerfile) so the
# binary runs on old and new distros alike. Needs: docker, dpkg-deb, tar.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
version=$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)$/\1/p' "$root/CMakeLists.txt" | head -1)
id=io.github.hillrush.HillRush
image=hillrush-build:1
stage="$root/build-release"
dist="$root/dist"
cache="$root/packaging/.cache"

echo "== Hill Rush $version"
rm -rf "$stage"
mkdir -p "$stage/out" "$dist"
umask 022

# --- tools ------------------------------------------------------------------
mkdir -p "$cache"
fetch() {
    local url=$1 file=$2 sha=$3
    if [ ! -f "$cache/$file" ]; then
        echo "downloading $file"
        curl -sSfL --retry 3 -o "$cache/$file" "$url"
    fi
    echo "$sha  $cache/$file" | sha256sum -c --quiet
}
fetch https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage \
    appimagetool-x86_64.AppImage ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
fetch https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 \
    runtime-x86_64 2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d

# --- compile + test in the portable build container ---------------------------
docker build -q -t "$image" -f "$root/packaging/Dockerfile" "$root/packaging" >/dev/null
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$root":/src:ro -v "$stage/out":/out \
    "$image" /src/packaging/container-build.sh

licenses="$stage/THIRD-PARTY-LICENSES.txt"
{
    echo "Hill Rush bundles the following third-party software."
    echo
    echo "=== raylib (zlib/libpng license) ==="; cat "$root/third_party/raylib/LICENSE"; echo
    echo "=== Box2D (MIT license) ==="; cat "$root/third_party/box2d/LICENSE"; echo
    echo "=== ENet (MIT license) ==="; cat "$root/third_party/enet/LICENSE"; echo
    echo "=== Lilita One font (SIL Open Font License 1.1) ==="; cat "$root/assets/fonts/OFL.txt"
} > "$licenses"

install_icons() {  # $1 = destination share dir
    for size in 32 48 64 128 256 512; do
        install -Dm644 "$stage/out/icons/$size.png" "$1/icons/hicolor/${size}x${size}/apps/$id.png"
    done
}

# --- AppImage -------------------------------------------------------------------
appdir="$stage/HillRush.AppDir"
install -Dm755 "$stage/out/hillrush" "$appdir/usr/bin/hillrush"
install -Dm644 "$root/packaging/linux/$id.desktop" "$appdir/usr/share/applications/$id.desktop"
install -Dm644 "$root/packaging/linux/$id.metainfo.xml" "$appdir/usr/share/metainfo/$id.appdata.xml"
install_icons "$appdir/usr/share"
install -Dm644 "$licenses" "$appdir/usr/share/doc/hillrush/THIRD-PARTY-LICENSES.txt"
cp "$root/packaging/linux/$id.desktop" "$appdir/$id.desktop"
cp "$stage/out/icons/256.png" "$appdir/$id.png"
ln -s usr/bin/hillrush "$appdir/AppRun"
appimage="HillRush-$version-x86_64.AppImage"
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e VERSION="$version" \
    -v "$stage":/stage -v "$cache":/tools:ro "$image" sh -c "
        cp /tools/appimagetool-x86_64.AppImage /tmp/appimagetool && chmod +x /tmp/appimagetool &&
        cd /tmp && /tmp/appimagetool --no-appstream --runtime-file /tools/runtime-x86_64 \
            /stage/HillRush.AppDir /stage/$appimage"
install -m755 "$stage/$appimage" "$dist/$appimage"

# --- .deb --------------------------------------------------------------------------
pkg="$stage/deb"
install -Dm755 "$stage/out/hillrush" "$pkg/usr/games/hillrush"
mkdir -p "$pkg/usr/share/applications"
sed 's|^Exec=.*|Exec=/usr/games/hillrush|' "$root/packaging/linux/$id.desktop" > "$pkg/usr/share/applications/$id.desktop"
install -Dm644 "$root/packaging/linux/$id.metainfo.xml" "$pkg/usr/share/metainfo/$id.metainfo.xml"
install_icons "$pkg/usr/share"
{
    echo "Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/"
    echo "Upstream-Name: Hill Rush"
    echo
    echo "Files: *"
    echo "License: All rights reserved."
    echo
    cat "$licenses"
} | install -Dm644 /dev/stdin "$pkg/usr/share/doc/hillrush/copyright"
mkdir -p "$pkg/DEBIAN"
cat > "$pkg/DEBIAN/control" <<CONTROL
Package: hillrush
Version: $version
Architecture: amd64
Maintainer: Hill Rush developers <hillrush@localhost>
Section: games
Priority: optional
Depends: libc6 (>= 2.28), libx11-6, libgl1, libxrandr2, libxinerama1, libxcursor1, libxi6
Recommends: libpulse0 | libasound2t64 | libasound2
Installed-Size: $(du -sk --exclude=DEBIAN "$pkg" | cut -f1)
Description: physics-based hill climb driving game
 Drive as far as you can over endless, ever steeper hills. Manage your fuel,
 collect coins, pull off flips and big air for bonuses, and spend your coins
 on upgrades, new vehicles and new stages.
CONTROL
deb="hillrush_${version}_amd64.deb"
dpkg-deb --root-owner-group -Zxz --build "$pkg" "$dist/$deb" >/dev/null

# --- tarball -------------------------------------------------------------------------
tarname="hillrush-$version-linux-x86_64"
tardir="$stage/$tarname"
install -Dm755 "$stage/out/hillrush" "$tardir/hillrush"
install -Dm755 "$root/packaging/install.sh" "$tardir/install.sh"
install -Dm755 "$root/packaging/uninstall.sh" "$tardir/uninstall.sh"
install -Dm644 "$root/packaging/linux/$id.desktop" "$tardir/$id.desktop"
for size in 32 48 64 128 256 512; do install -Dm644 "$stage/out/icons/$size.png" "$tardir/icons/$size.png"; done
install -Dm644 "$licenses" "$tardir/THIRD-PARTY-LICENSES.txt"
install -Dm644 "$root/packaging/README-tarball.txt" "$tardir/README.txt"
tar -C "$stage" --owner=0 --group=0 -czf "$dist/$tarname.tar.gz" "$tarname"

(cd "$dist" && sha256sum "$appimage" "$deb" "$tarname.tar.gz" > SHA256SUMS)
echo "== done"
ls -la "$dist"
