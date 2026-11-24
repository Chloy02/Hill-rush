#!/bin/bash
# Smoke-tests the release packages in clean containers of several distros:
# the binary must load, run the headless simulation, and render real frames
# (OpenGL via Mesa's software renderer under Xvfb). Debian-family images also
# install the .deb through apt to check its dependencies.
set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
dist="$root/dist"
version=$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)$/\1/p' "$root/CMakeLists.txt" | head -1)
tarball="hillrush-$version-linux-x86_64.tar.gz"
deb="hillrush_${version}_amd64.deb"
appimage="HillRush-$version-x86_64.AppImage"
out="$root/build-release/smoke"
mkdir -p "$out"

images=("ubuntu:20.04" "ubuntu:22.04" "ubuntu:24.04" "debian:11" "debian:12" "fedora:41" "archlinux:latest")
[ $# -gt 0 ] && images=("$@")

# A tiny automation script: render a few frames of real gameplay and screenshot.
cat > "$out/smoke.txt" <<'SCRIPT'
wait 60
press gas
wait 120
screenshot frame
quit
SCRIPT

pass=0
fail=0
for image in "${images[@]}"; do
    tag=$(echo "$image" | tr ':/' '__')
    archive=''
    if [ "$image" = "debian:11" ]; then
        # Bullseye's LTS has ended; its packages now live on archive.debian.org.
        archive='printf "deb http://archive.debian.org/debian bullseye main\ndeb http://archive.debian.org/debian-security bullseye-security main\n" > /etc/apt/sources.list;'
    fi
    case "$image" in
        ubuntu:*|debian:*)
            setup=$archive'export DEBIAN_FRONTEND=noninteractive; apt-get -o Acquire::Check-Valid-Until=false update -qq >/dev/null &&
                   apt-get install -y -qq xvfb xauth libgl1-mesa-dri >/dev/null &&
                   apt-get install -y -qq /dist/'"$deb"' >/dev/null && echo "deb installed: $(dpkg -s hillrush | grep Version)"'
            bin=/usr/games/hillrush ;;
        fedora:*)
            setup='dnf -y -q install xorg-x11-server-Xvfb mesa-dri-drivers mesa-libGL libX11 libXrandr libXinerama libXcursor libXi xauth >/dev/null'
            bin=/tmp/pkg/hillrush ;;
        archlinux:*)
            setup='pacman -Sy --noconfirm --quiet xorg-server-xvfb mesa libx11 libxrandr libxinerama libxcursor libxi xorg-xauth >/dev/null'
            bin=/tmp/pkg/hillrush ;;
    esac
    echo "=== $image"
    docker run --rm -e HOST_IDS="$(id -u):$(id -g)" -v "$dist":/dist:ro -v "$out":/out "$image" bash -c "
        set -e
        mkdir -p /out/$tag
        trap 'chown -R \$HOST_IDS /out/$tag' EXIT  # don't leave root-owned files behind
        $setup
        mkdir -p /tmp/pkg && tar -xzf /dist/$tarball -C /tmp/pkg --strip-components=1
        glibc=\$(ldd --version | head -1)
        echo \"glibc: \$glibc\"
        $bin --version
        $bin --sim --seconds 20 | head -c 160; echo
        cd /tmp && /tmp/pkg/hillrush --version >/dev/null
        mkdir -p /out/$tag
        xvfb-run -a -s '-screen 0 1280x720x24' $bin --data-dir /tmp/data --mute --fixed-step \
            --play countryside:jeep --script /out/smoke.txt --screenshot-dir /out/$tag 2>&1 | grep -E 'ERROR|running' || true
        test -s /out/$tag/frame.png && echo 'rendered frame OK'
        cd /tmp && cp /dist/$appimage . && ./$appimage --appimage-extract-and-run --version
    " > "$out/$tag.log" 2>&1
    status=$?
    sed 's/^/   /' "$out/$tag.log" | tail -8
    if [ $status -eq 0 ]; then pass=$((pass + 1)); echo "   PASS"; else fail=$((fail + 1)); echo "   FAIL ($status)"; fi
done
echo "== $pass passed, $fail failed"
[ $fail -eq 0 ]
