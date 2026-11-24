#!/bin/bash
# Plays a real three-player online race on this computer: three copies of the
# game, each on its own virtual display (Xvfb), all driven by the autopilot.
#   ANNA hosts (jeep), BEN joins by address (dirt bike),
#   CARA starts at the title screen and finds the room on the network (monster truck).
# They race to 500 m, look at the results, and the host starts a rematch.
# Screenshots from every player end up in build/online-test/shots.
# Needs: a built game (./run.sh or cmake --build build) and Xvfb.
set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
bin="$root/build/bin/hillrush"
work="$root/build/online-test"
[ -x "$bin" ] || { echo "Build the game first (./run.sh)"; exit 1; }
command -v Xvfb >/dev/null || { echo "Needs Xvfb (sudo apt install xvfb)"; exit 1; }

rm -rf "$work" && mkdir -p "$work"/{a,b,c,shots}
cd "$work" || exit 1

displays=(171 172 173)
xpids=()
for d in "${displays[@]}"; do
    Xvfb ":$d" -screen 0 1280x720x24 -nolisten tcp >/dev/null 2>&1 &
    xpids+=($!)
done
trap 'kill "${xpids[@]}" 2>/dev/null' EXIT
for d in "${displays[@]}"; do
    for _ in $(seq 1 30); do xdpyinfo -display ":$d" >/dev/null 2>&1 && break; sleep 0.2; done
done

# Logical click targets (1280x720 layout): room goal button, START / READY,
# title ONLINE RACE, first room's JOIN, pause menu MAIN MENU, results RACE AGAIN.
cat > a.txt <<'EOF'
waitfor room 600
wait 30
click 1136 432
wait 8
click 1136 432
wait 8
click 1136 432
wait 8
click 1136 432
waitstart 3 6000
wait 20
screenshot anna-room
click 1100 661
waitfor online-race 600
wait 15
screenshot anna-countdown
wait 150
screenshot anna-race
waitfor online-results 9000
wait 60
screenshot anna-results
click 510 556
waitfor room 600
wait 20
screenshot anna-rematch-room
quit
EOF
cat > b.txt <<'EOF'
waitfor room 900
wait 30
click 1100 661
waitfor online-race 9000
wait 200
screenshot ben-race
waitfor online-results 9000
wait 30
screenshot ben-results
waitfor room 3000
quit
EOF
cat > c.txt <<'EOF'
wait 60
tap pause
wait 20
click 640 528
waitfor title 300
wait 20
click 640 504
waitfor online-menu 300
wait 60
screenshot cara-finds-room
click 704 263
waitfor room 900
wait 20
click 1100 661
waitfor online-race 9000
wait 250
screenshot cara-race
waitfor online-results 9000
wait 30
screenshot cara-results
waitfor room 3000
quit
EOF
printf 'version=1\nselected.vehicle=bike\nvehicle.bike.unlocked=1\n' > b/save.txt
printf 'version=1\nselected.vehicle=monster\nvehicle.monster.unlocked=1\n' > c/save.txt

common=(--autoplay --mute --windowed --size 960x540 --screenshot-dir "$work/shots")
DISPLAY=":${displays[0]}" "$bin" --data-dir "$work/a" --name ANNA --host "${common[@]}" --script a.txt \
    --telemetry a/telemetry.jsonl > a/out.log 2>&1 &
pa=$!
sleep 2
DISPLAY=":${displays[1]}" "$bin" --data-dir "$work/b" --name BEN --join 127.0.0.1 "${common[@]}" --script b.txt \
    --telemetry b/telemetry.jsonl > b/out.log 2>&1 &
pb=$!
# CARA's --autoplay starts a single-player autopilot run; her script leaves it for the title screen.
DISPLAY=":${displays[2]}" "$bin" --data-dir "$work/c" --name CARA "${common[@]}" --script c.txt \
    --telemetry c/telemetry.jsonl > c/out.log 2>&1 &
pc=$!

fail=0
for p in "ANNA:a:$pa" "BEN:b:$pb" "CARA:c:$pc"; do
    IFS=: read -r name dir pid <<< "$p"
    if wait "$pid"; then echo "   $name: PASS"; else echo "   $name: FAIL (see $work/$dir/hillrush.log)"; fail=1; fi
done
grep -h "Online race\|Race over\|joined\|script line.*gave up\|expected screen" a/hillrush.log b/hillrush.log c/hillrush.log \
    | sed 's/^/   /' | head -20
echo "== screenshots: $work/shots"
exit $fail
