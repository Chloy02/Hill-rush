# Hill Rush

A physics hill-climb driving game for Linux and Windows, in the spirit of *Hill Climb Racing*.
Drive as far as you can over endless, ever-steeper hills, manage your fuel,
grab coins, pull off flips and big air, and spend your coins on upgrades,
new vehicles and new stages. Or race up to three friends online.

All art and sound are original: the graphics are drawn procedurally and every
sound effect, the engine and the music are synthesised at start-up. Nothing is
loaded from disk except your save file.

## Play

From this folder, just run:

```sh
./run.sh
```

It rebuilds the game first if CMake is installed (like `npm run dev`), then
starts it. Any option works, e.g. `./run.sh --play desert:bike`.

Or use one of the packages in `dist/`:

| File | Use it on | How |
|---|---|---|
| `HillRush-1.1.0-x86_64.AppImage` | any distro | `chmod +x HillRush-*.AppImage && ./HillRush-*.AppImage` |
| `hillrush_1.1.0_amd64.deb` | Debian, Ubuntu, Mint, Pop!_OS | `sudo apt install ./hillrush_1.1.0_amd64.deb`, then start *Hill Rush* from the menu |
| `hillrush-1.1.0-linux-x86_64.tar.gz` | any distro, no root | unpack, then `./hillrush` or `./install.sh` for a menu entry |
| `HillRush-1.1.0-windows-x64.zip` | Windows 10 / 11 (64-bit) | unzip, double-click `HillRush.exe` (SmartScreen: *More info* → *Run anyway*, it isn't code-signed) |

The binary needs only glibc 2.28+, X11 (or XWayland) and OpenGL 3.3, so it
runs on Ubuntu 20.04+, Debian 10+, Fedora, Arch, openSUSE and friends.

### Controls

| | Keyboard | Gamepad | Mouse / touch |
|---|---|---|---|
| Gas | Right / D | A, right trigger | GAS pedal |
| Brake / reverse | Left / A | X, left trigger | BRAKE pedal |
| Nitro boost / Thrusters | Space / W / Up | Y | NITRO / THRUST pedal |
| Pause | Esc / P | Start | pause button |
| Restart run | R | Back | |
| Fullscreen | F11 / Alt+Enter | | |
| Screenshot | F12 | | |

Menus work with the mouse, or with arrow keys + Enter (Esc goes back).

In the air, **gas tips the car backwards** and **brake tips it forwards** —
that is how you land on your wheels (and how you do flips).

### Difficulty

Choose **Easy**, **Medium** or **Hard** in *Settings* (or `--difficulty hard`).

| | Easy | Medium | Hard |
|---|---|---|---|
| Ground | smooth hills | rough, bumpy | wild, jumpy |
| Fuel tank | 100% | 80% | 65% |
| Fuel cans spread out | normally | 2x faster | 3x faster |
| Coins and level rewards | x1 | x1.5 | x2 |

The big hills and the slope limits are the same in every mode, so harder modes
add no impassable walls - just more ways to crash and less fuel to waste.
Records and level checkpoints are kept separately for each difficulty, and
vehicle handling never changes.

### Play online with friends

Race up to three friends (four players) on the same Wi-Fi or network.
Linux and Windows players can race each other; everyone needs the same version.

1. **One player hosts.** Main menu → **ONLINE RACE** → type your name → **HOST A RACE**.
   In your room, choose the stage, the goal (500 m to 5,000 m, or *furthest
   wins*) and the difficulty.
2. **Friends join.** Main menu → **ONLINE RACE**. The host's room shows up
   under *Races on your network* → **JOIN**. If it doesn't show up, type the
   address printed at the top of the host's room (e.g. `192.168.1.20`) under
   *Join by address* → **JOIN**.
3. **Everyone gets ready.** Pick your car with the arrows next to your name
   (you drive with your own upgrades), then press **READY!**
4. **The host presses START RACE.** 3, 2, 1, GO! First to the finish wins.
   Once someone finishes, the others have 30 seconds to get there. Crash or
   run out of fuel and you watch the others. Coins you collect are yours to keep.
5. **Rematch.** On the results screen the host presses **RACE AGAIN**.

Other players' cars have name tags. Arrows at the screen edge point to anyone
off-screen, the leaderboard on the left shows positions live, and the badge
at the top right shows the connection (ping).
Cars pass through each other, so nobody can knock you off the track.

Shortcuts from a terminal:

```sh
./run.sh --host                      # open a room straight away
./run.sh --join 192.168.1.20         # join a friend's room
./run.sh --name RISHI                # set your online name
```

To try it on one computer, run the game twice. Give the second copy its own
save folder:

```sh
./run.sh --host                                      # first terminal
./build/bin/hillrush --join 127.0.0.1 --name PLAYER2 --data-dir /tmp/hillrush-p2   # second
```

If a room doesn't show up or you can't join:

* The game uses **UDP ports 47820** (finding rooms) **and 47821** (racing).
* **Windows:** the first time you host, Windows Firewall asks whether Hill Rush
  may use the network. Tick *Private networks* and choose *Allow access*.
* **Linux with a firewall** (e.g. `ufw`): `sudo ufw allow 47820:47821/udp`
* Guest Wi-Fi or office networks often block players from seeing each other.
  Join by address, or use a phone hotspot.
* **Over the internet:** put everyone on one virtual network with a free tool
  like [Tailscale](https://tailscale.com) or [ZeroTier](https://www.zerotier.com),
  then join by the host's Tailscale address. Or forward UDP port 47821 on the
  host's router and join with the host's public IP address.

### Rules

* Fuel drains with time. Grab the red fuel cans to refill; they get further
  apart the further you go.
* If the driver's head touches the ground it's **Driver Down**.
* Coins come in clusters every 30 m and are worth more the further you get.
* Bonuses: air time (from 2 s), wheelies (from 2 s), **FLIP** / **BACK FLIP**
  (+1000 each), **CLEAN LANDING** (+500 per flip), **LUCKY FUEL** (+1000 when
  refuelling while coasting on empty), and a one-time **NECK FLIP** (+2500).
* Stunts and coins charge the **Nitro Boost** bar (`Space` / `W` / `Up`) on
  standard vehicles; on the **Moonlander**, `Space` fires downward **Rocket
  Thrusters** (burning extra fuel). The **Rally Car** has a breakable **Roll
  Cage** that absorbs roof impacts before snapping off.
* Every stage has level checkpoints (every 200 m on Countryside) that pay a
  one-time reward.
* Upgrades cost `level² × 1000` coins (more for some parts).

Progress is saved to `~/.local/share/hillrush/save.txt` (or `$XDG_DATA_HOME/hillrush`),
and on Windows to `%APPDATA%\HillRush\save.txt`. Screenshots (F12) go to a
`screenshots` folder next to the save.

## Build from source

Requirements: a C++20 compiler (GCC 11+ or Clang 14+), CMake 3.22+, and the
X11/OpenGL development headers (`libx11-dev libxrandr-dev libxinerama-dev
libxcursor-dev libxi-dev libgl-dev` on Debian/Ubuntu). raylib, Box2D and
ENet are vendored in `third_party/`, so nothing else is downloaded.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bin/hillrush_tests     # unit + physics tests
./build/bin/hillrush           # play
```

### Release packages (all platforms at once)

```sh
./scripts/build-all.sh        # Linux AppImage/.deb/.tar.gz + Windows .zip, one SHA256SUMS
./scripts/test-packages.sh    # install + run + render on 7 Linux distros (Docker)
./scripts/test-windows.sh     # run the Windows build's tests and a game session under Wine
./scripts/test-online.sh      # three players race online on this computer (needs Xvfb)
```

* Linux builds happen inside an AlmaLinux 8 container (glibc 2.28) for maximum
  compatibility, and the unit tests run there first.
* The Windows build is cross-compiled with MinGW-w64 in an Ubuntu container
  into a single static `HillRush.exe` (only standard Windows DLLs needed).
* Needs Docker, `dpkg-deb` and `tar` on the host.

### Useful command-line options

```
hillrush --help                       all options
hillrush --windowed --size 1600x900   window size
hillrush --play desert:bike           skip the menus (locked content = test drive, nothing saved)
hillrush --host / --join ADDRESS      online race: open a room / join one (--name NAME)
hillrush --autoplay                   let the autopilot drive (test drive; add --stunts for flips)
hillrush --sim --stage moon --vehicle racecar --seconds 300
                                      headless bot run, prints JSON stats
hillrush --terrain-stats arctic       slope statistics of a stage
hillrush --climb-test --vehicle jeep --upgrades 13,1,1,1
                                      steepest ramp a setup can climb
```

Automated play-tests: `--script FILE` drives the real game (key presses,
clicks, screenshots, screen assertions — see `src/game/Automation.hpp`), and
`--telemetry FILE` logs the game state as JSON lines.

## Project layout

```
src/core/    math, noise, RNG, logging, paths
src/sim/     game rules and physics (Box2D) - no graphics, unit-tested headless
src/net/     online races: rooms, discovery, car snapshots (ENet) - no graphics, unit-tested
src/game/    rendering, HUD, menus, input, audio, automation (raylib)
tests/       unit and physics tests
packaging/   desktop entry, AppStream metadata, Dockerfile, install scripts
scripts/     release build
third_party/ raylib 6.2, Box2D 3.1.1, ENet 1.3.18 (vendored)
docs/        design notes
```

See [docs/DESIGN.md](docs/DESIGN.md) for architecture and tuning notes.

## Credits

* [raylib](https://www.raylib.com/) 6.2 — zlib licence
* [Box2D](https://box2d.org/) 3.1.1 — MIT licence
* [ENet](http://enet.bespin.org/) 1.3.18 — MIT licence
* [Lilita One](https://fonts.google.com/specimen/Lilita+One) font by Juan Montoreano — SIL Open Font Licence 1.1

Inspired by *Hill Climb Racing* by Fingersoft. Hill Rush is an independent
fan project with no affiliation; it uses none of the original game's assets.
