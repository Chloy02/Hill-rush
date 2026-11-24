#!/bin/bash
# Smoke-tests the Windows build under Wine with a virtual display: runs the
# unit tests as a Windows program, then plays a few seconds and screenshots it.
set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
work="$root/build-windows/wine"
rm -rf "$work" && mkdir -p "$work"
cp "$root/build-windows/out/hillrush_tests.exe" "$root"/build-windows/out/HillRush-*-windows-x64/HillRush.exe "$work/"
printf 'wait 60\npress gas\nwait 150\nscreenshot frame\nwait 30\nrelease gas\nquit\n' > "$work/smoke.txt"

docker build -q -t hillrush-wine-test:2 "$root/packaging/wine-test" >/dev/null
docker run --rm -e HOST_IDS="$(id -u):$(id -g)" -v "$work":/w hillrush-wine-test:2 bash -c '
    trap "chown -R \$HOST_IDS /w" EXIT
    W=/usr/lib/wine/wine64
    cd /w
    xvfb-run -a -s "-screen 0 1280x720x24" sh -c "
        $W wineboot -i >/dev/null 2>&1
        echo == unit tests under Wine:
        $W hillrush_tests.exe 2>&1 | tail -1
        echo == game under Wine:
        $W HillRush.exe --data-dir C:\\\\hrdata --mute --fixed-step --play countryside:jeep \
            --script Z:\\\\w\\\\smoke.txt --screenshot-dir Z:\\\\w\\\\shots; echo game exit code: \$?
    "
    grep -E "running|ERROR|Saved screenshot" "$WINEPREFIX/drive_c/hrdata/hillrush.log" | tail -4
' 2>&1 | grep -vE "fixme|^$"
test -s "$work/shots/frame.png" && echo "== Windows build rendered a frame: $work/shots/frame.png"
