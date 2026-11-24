# Hill Rush — design notes

## Goals

A faithful-feeling Hill Climb Racing–style game for Linux desktops:
the same core loop (drive right, manage fuel, don't land on your head, spend
coins on upgrades), original art and sound, and packages that install cleanly
on any mainstream distro.

## Technology choices

| Decision | Why |
|---|---|
| **C++20 + raylib 6.2 + Box2D 3.1.1** | A native binary with no runtime: ~3 MB, instant start-up. Box2D's wheel joints (spring + motor) are exactly how this genre's vehicles work. raylib covers windowing, GL rendering, audio and gamepads in one small C library. Game engines (Godot, Unity) would add 70+ MB and a separate physics engine with weaker joint stability; Electron would add 100+ MB. |
| Box2D **3.1.1**, not 3.2.0 | 3.2.0 shipped days before this project with breaking joint/chain/contact changes. 3.1.1 has been stable since mid-2025. |
| raylib **6.2** | Bug- and security-fix release on top of 6.0 (April 2026). |
| GLFW **X11 only** | Works natively on X11 and through XWayland on every Wayland desktop, without depending on libdecor or wayland-scanner. |
| Vendored dependencies | Builds offline and reproducibly; versions are pinned in `third_party/`. |
| Build in **AlmaLinux 8** (glibc 2.28), `-static-libstdc++ -static-libgcc` | The binary only needs glibc, libm, libX11 and (dlopened) libGL / audio libraries, so it runs on Ubuntu 20.04+, Debian 10+ and everything newer. |
| AppImage + .deb + tarball | AppImage for "download and run anywhere", .deb for Debian-family integration, tarball with a no-root `install.sh` for everyone else. Flatpak was skipped for v1 (needs Flathub's build infrastructure and a several-hundred-MB runtime). |
| Windows: MinGW-w64 cross-build | Same code, cross-compiled in a container into one static `HillRush.exe` (only standard Windows DLLs). Icon + version info via a resource script; the icon is named `GLFW_ICON`, so the game window itself (taskbar, Alt+Tab, title bar) gets it at every size from the start. GUI subsystem (no console). Saves go to `%APPDATA%\HillRush`; the crash-safe save uses `MoveFileEx(..., WRITE_THROUGH)`. Tested under Wine (unit tests + a scripted, rendered game session). |
| Fullscreen via GLFW directly | raylib's borderless toggle also flips window decorations, which left GNOME windows stuck at full size; the game drives `glfwSetWindowMonitor` itself, re-asserts the windowed size afterwards, and every frame keeps raylib's idea of the window size equal to the real one. |
| ENet 1.3.18 for online races | Small, battle-tested reliable-UDP library (MIT), plain C, builds on Linux and MinGW. Gives connections, timeouts, a reliable channel for room state and an unreliable channel for car snapshots, which is everything a race needs. Valve's GameNetworkingSockets or Steam would be far heavier; raw sockets would mean reinventing reliability. |
| One build for everything | `scripts/build-all.sh` rebuilds the Linux and Windows packages together and writes a single `SHA256SUMS`, so packages never drift apart. |
| Window icons | On Linux the window carries the icon at 7 sizes (16–256 px) for docks that use it directly (AppImage, `./run.sh`); installed copies match the launcher through `StartupWMClass=Hill Rush`, the window's X11 class, so docks show the installed theme icon. |
| Everything embedded | The font is compiled in (`cmake/EmbedFiles.cmake`); textures, icon, sound and music are generated at start-up. The only file the game writes is the save (`~/.local/share/hillrush/save.txt`, written atomically). |

## Architecture

```
src/core   noise, RNG, math, logging, XDG paths            (no dependencies)
src/sim    Defs, Terrain, Vehicle, Pickups, Session,        (Box2D only)
           Bot, Progress, Pose, SimHarness
src/net    Protocol, Online (rooms), Discovery, Race       (ENet, kept private)
src/game   App, screens, LiveRun, WorldView, VehicleArt,    (raylib)
           Hud, Ui, Input, Audio, Particles, Automation
```

* **sim** has no graphics, so the rules and physics are unit-tested and
  soak-tested headless (`hillrush_tests`, `hillrush --sim`), at roughly
  2000× real time.
* **Session** is one run. It owns the Box2D world, steps it at a fixed
  60 Hz (8 sub-steps) and emits `GameEvent`s (coin, fuel, flip, crash...).
  The play screen turns events into sound, particles and banners.
* Rendering interpolates between the last two physics states, so motion is
  smooth at any refresh rate.
* **App** owns the window, assets, audio, input, UI and save data, and swaps
  screens (title, stages, garage, play, settings). Screen switches are deferred
  to the start of the next frame, because buttons are handled while a screen draws.
* **Automation** feeds scripted input through the real `Input` object, so
  automated play-tests exercise the same code paths as a player.

## Terrain

* Height = three Perlin octaves whose amplitudes ramp up with distance
  (an ease-out curve), sharpened by a tanh S-curve that grows steeper further
  out. That flattens hilltops and valleys and steepens the faces between them.
* The flat start pad is 18 m. Behind the start is a steep wall plus an
  invisible stop.
* **Slope cap**: samples are produced in order from the start line, and each
  0.5 m step may rise or fall at most `tan(maxSlope(distance))`. That keeps
  every hill climbable by an upgraded vehicle with momentum (no dead ends),
  while staying fully deterministic.
* Physics streams the ground as Box2D chain shapes in 64 m chunks. Box2D v3
  chains collide on one side only (to the right of their direction), so points
  run right-to-left. Each chunk also borrows one ghost vertex from each
  neighbour, so wheels roll over the seams without bumps.
* Arctic ice patches are per-segment chain materials. Moon craters are
  bowls with raised rims, added on top.
* Every stage has a fixed seed, so the track is the same every run, as in the
  original, and best distances are comparable.

## Vehicles

* The chassis is convex polygons. Wheels are circles on **wheel joints**: a
  vertical axis, spring (Hz) + damping, travel limits, and a motor per wheel.
  The driver's head is its own body on a sprung, limited revolute "neck";
  the head touching the ground ends the run.
* Engine torque is **split** across the driven wheels. 4WD shares it with the
  front axle instead of adding torque, so it helps traction without extra
  wheelies.
* The brake pedal brakes all wheels, then reverses.
* **Air control** is an angular acceleration plus a spin-rate target
  (torque = a·I·pedal − damping·ω, where I is the whole assembly's inertia).
  Gas tips the car backwards (back flip) and brake forwards, as in the
  original. A small fraction applies on the ground.
* Friction mixing is the product of tyre and surface grip, so "ice has a
  tenth of the grip" reads naturally.
* The chassis:wheel mass ratio is kept around 4–7:1, which keeps the joints
  stiff (a recommendation from Box2D sample measurements).

### Tuning method

1. `--climb-test` reports the steepest ramp a vehicle/upgrade combination can
   climb, from rest and with a 40 m run-up. Stock torque was set below each
   vehicle's wheelie limit, so engine upgrades measurably improve climbing:
   the jeep goes from 25° from rest stock to about 38° with a maxed engine.
2. `--terrain-stats` reports slope percentiles per 500 m band for each stage.
3. `--sim` runs the bot over every stage with stock, mid and maxed upgrades.
   The stock jeep's bot (a near-perfect driver) ends at 1.3–3.5 km depending
   on the stage; upgrades extend that to 4–8 km. Human players land well below
   the bot.

## Game rules (taken from the original)

| Rule | Value |
|---|---|
| Fuel | Drains by time; jeep tank lasts 40 s. "FUEL LOW" at 20%. When empty: no drive, brakes or air control. The car coasts until it stops (or 6 s); a fuel can reached while coasting saves the run. |
| Fuel cans | Stage-specific spacing: every 90 m with slow growth on most stages; quadratic growth on Highlands and Mars. |
| Coins | A cluster every 30 m, worth `coinRate × distance` (max 7000), paid largest coin first (500/100/25/5) and laid out high to low. |
| Air time | Starts at 2 s, steps every 0.25 s: 25, 50, 75, 100 (AIR TIME), 200–500 (BIG AIR TIME), then 1000 and +500 per step (INSANE AIR TIME); max 25 000. |
| Flips | FLIP / BACK FLIP at every 270° (+360°) of rotation, 1000 coins each. NECK FLIP: crash during, or within 1 s of, a flip; 2500 coins, once per run. |
| Levels | Countryside, Desert, Arctic: every 200 m. Highlands: 400 m. Moon, Mars: 300·L − 100. Reward 3000·L + 2000, paid once per stage. |
| Upgrades | Level L costs `c·L²`; c = 1000 by default, 1500–3000 for some parts. Max levels 10–20, as in the original. |
| Prices | Bike 75k, Monster Truck 100k, Tractor 100k, Race Car 250k. Desert 35k, Arctic 75k, Highlands 125k, Moon 175k, Mars 250k. |

Coins collected are kept even if you restart or quit mid-run.

## Difficulty levels

Easy is the original tuning, untouched (a unit test checks the terrain is
identical sample for sample). Medium and Hard change three things:

* **Rough ground**: extra bumps (0.5 m rising to 1.4 m, x1 on Medium, x2 on
  Hard) that throw the car around and cause flips. They fade out wherever the
  stage's own terrain is steep or is about to climb steeply: a bump-and-dip at
  the foot of a big hill kills the car's momentum and turns the hill into a
  dead end, which bot runs showed clearly.
* **Fuel**: 80% / 65% tank, and the gaps between cans widen 2x / 3x faster.
* **Rewards**: coins and level rewards x1.5 / x2. Reward multipliers are the
  usual way to make harder modes worth playing.

What was tried first and rejected: bringing the big hills in sooner and
making them taller and steeper, with a higher slope cap. It created
pit-plus-wall dead ends around 400 m that no upgrade could pass.

Measured with the clumsy bot (median of 5 runs, stock / mid-upgraded jeep):

| Stage | Easy | Medium | Hard |
|---|---|---|---|
| Countryside | 1672 / 2579 m | 935 / 2139 m | 864 / 924 m |
| Desert | 7554 / 9055 m | 1898 / 3342 m | 1349 / 1863 m |
| Arctic | 3246 / 5365 m | 1784 / 2467 m | 1218 / 1782 m |
| Mars | 2009 / 2027 m | 958 / 1098 m | 792 / 800 m |

Records and level checkpoints are stored per difficulty. Easy keeps the
original save keys, so older saves load as Easy progress.

## Online races

* **Model.** Each player drives only their own car, locally, with full
  physics. About 20 times a second it sends a snapshot of the car: race time,
  distance, state and the pose of the body, wheels and head. Cars don't
  collide, as in Hill Climb Racing 2, so there is no shared physics, no
  lockstep and no rollback. Physics doesn't have to match bit for bit across
  machines, so Linux and Windows players can race each other.
* **Same track everywhere.** Terrain and pickups come from the stage seed
  plus the room's difficulty, so every player builds the identical track
  locally. No map data goes over the network.
* **Host-authoritative room, star topology.** The host keeps the room state:
  players, colours, vehicles, ready flags, settings, phase and results. It
  sends the full state reliably whenever something changes (4 players at
  most, so it's tiny) and relays every snapshot to the other players. Guests
  apply their own lobby changes optimistically until the host confirms them.
* **Channels.** ENet channel 0 is reliable (hello/welcome/reject, room state,
  player updates, "my run ended" reports). Channel 1 is unreliable and
  sequenced (snapshots: a late one is simply dropped).
* **Smooth remote cars.** Snapshots are buffered per car, and each car is
  drawn 0.1 s in the past, interpolated between the two snapshots around that
  time. Remote cars are drawn behind your own, with a name tag, or an
  arrow at the screen edge when they're off screen.
* **Race flow.** Lobby → 3 s countdown (cars wait on the line, no fuel burns)
  → racing → results → back to the room. The finish is the room's goal
  distance (or none: furthest wins). The first finish starts a 30 s grace
  period; the race also ends when nobody is driving. Order: finishers by time,
  then everyone else by distance, then players who left. Online runs pay no
  level rewards and set no records, but the coins collected are banked.
* **Finding rooms.** A plain UDP query on port 47820 goes to the broadcast
  address, to 127.0.0.1 and to the local /24 network. Hosts answer with a small
  room advert (name, players, stage, phase, port and a random token, so a
  room that answers on two addresses is listed once). Joining by address
  works without discovery (Tailscale, port forwarding, guest networks).
* **Robustness.** Every packet is length- and range-checked; NaN or huge
  floats are clamped; names are sanitised; unknown stages or vehicles fall
  back to defaults. Peers time out after 3–8 s instead of ENet's 30. The
  handshake carries a protocol version, so different game versions get a
  clear message instead of strange behaviour. A full room, a race in
  progress or a closed room each get a clear message too. Every snapshot
  carries its race number, so a late one from the last race can't land in
  the rematch. Lobby changes carry a sequence number that the host echoes,
  so a guest's screen never disagrees with the room. Finish times far from
  the host's own race clock are pulled back into range. Connections that
  never say hello are dropped after 5 s, and packets are capped at 64 KB.
* **Why ENet stays private.** Its Windows headers include `windows.h`, whose
  names (`Rectangle`, `CloseWindow`, `DrawText`...) clash with raylib's. Only
  `src/net/*.cpp` sees ENet; the game uses plain C++ interfaces.

## Audio

All synthesised (`src/game/Audio.cpp`):

* **Engine**: a live audio-thread stream. Firing pulses at 26–150 Hz plus a
  half-order component and pulse-gated noise, through two low-pass filters
  whose cutoff opens with throttle, an exhaust comb filter, a DC blocker and
  soft saturation. Parameters cross threads through atomics and are smoothed
  per sample.
* **Effects**: additive and subtractive one-shots (coin, fuel, crash,
  landing, flip arpeggio, low-fuel beeps, UI click, purchase, game-over,
  fanfare).
* **Music**: a country-blues loop (G–C–G–D…) of Karplus-Strong plucked bass,
  chord strums and a banjo-ish lead, with a soft kick and brush, rendered once
  and streamed as a seamless loop.

`hillrush --export-audio DIR` writes everything to WAV for inspection.

## Testing

* `hillrush_tests` (network): message encoding and malformed packets, race
  order, snapshot interpolation, and real host/guest sessions over loopback:
  joining, lobby changes, a full race with results, the finish grace period,
  full rooms, version mismatches, mid-race joins, players leaving, the host
  closing the room, and discovery.
* Multi-instance play-tests: three game instances on separate virtual
  displays (host, guest by address, guest by discovery) race to the finish
  on autopilot, with screenshots from each.
* `hillrush_tests`: noise, terrain continuity and slope limits, fuel/coin/
  air-time/level/upgrade rules, save round-trips and corrupted-save handling,
  vehicles settling, driving, running out of fuel, Driver Down, an airborne
  back flip, one-time level rewards, and bot runs on every stage.
* Headless bot sims across stages and upgrade levels.
* Scripted visual play-tests under Xvfb, with screenshots reviewed after each
  build: menu flow by mouse and by keyboard, purchases, driving, pause,
  crash, results, retry, and every stage/vehicle pair.
* Runs on a real GPU desktop (60 FPS, audio device).
* Package smoke tests in clean containers of several distributions.
