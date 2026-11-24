#!/bin/bash
# Build (if needed) and start the game - the "npm run dev" of this project.
#   ./run.sh                    play
#   ./run.sh --play desert:bike any game option works (see ./run.sh --help)
# Uses the system CMake if there is one; otherwise it sets up CMake + Ninja in
# .tools/ (a Python virtualenv) the first time, so nothing needs to be installed.
set -e
cd "$(dirname "$0")"

if ! command -v cmake >/dev/null 2>&1; then
    if [ ! -x .tools/venv/bin/cmake ]; then
        echo "Setting up CMake and Ninja in .tools/ (one time)..."
        python3 -m venv .tools/venv
        .tools/venv/bin/pip install --quiet cmake ninja
    fi
    export PATH="$PWD/.tools/venv/bin:$PATH"
fi

generator=()
command -v ninja >/dev/null 2>&1 && generator=(-G Ninja)
if [ ! -f build/CMakeCache.txt ]; then
    cmake -S . -B build "${generator[@]}" -DCMAKE_BUILD_TYPE=RelWithDebInfo
fi
cmake --build build -j"$(nproc)"

exec ./build/bin/hillrush "$@"
