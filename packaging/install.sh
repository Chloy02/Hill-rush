#!/bin/sh
# Installs Hill Rush for the current user (no root needed):
#   ~/.local/bin/hillrush, a menu entry and icons.
set -eu
ID=io.github.hillrush.HillRush
src=$(cd "$(dirname "$0")" && pwd)
data=${XDG_DATA_HOME:-$HOME/.local/share}
bin=$HOME/.local/bin

install -Dm755 "$src/hillrush" "$bin/hillrush"
for size in 32 48 64 128 256 512; do
    install -Dm644 "$src/icons/$size.png" "$data/icons/hicolor/${size}x${size}/apps/$ID.png"
done
mkdir -p "$data/applications"
# Desktop files can't expand ~ and ~/.local/bin is often not on the session PATH.
sed "s|^Exec=.*|Exec=\"$bin/hillrush\"|" "$src/$ID.desktop" > "$data/applications/$ID.desktop"
chmod 644 "$data/applications/$ID.desktop"

# Only refresh an icon cache that already exists: creating a new one in the
# user's icon dir could later go stale and hide other applications' icons.
if [ -f "$data/icons/hicolor/icon-theme.cache" ] && command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f "$data/icons/hicolor" || true
fi

echo "Hill Rush installed."
echo "Start it from your applications menu, or run: $bin/hillrush"
