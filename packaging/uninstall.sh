#!/bin/sh
# Removes a user-level install made by install.sh. Your save game is kept.
set -eu
ID=io.github.hillrush.HillRush
data=${XDG_DATA_HOME:-$HOME/.local/share}

rm -f "$HOME/.local/bin/hillrush" "$data/applications/$ID.desktop"
for size in 32 48 64 128 256 512; do
    rm -f "$data/icons/hicolor/${size}x${size}/apps/$ID.png"
done
if [ -f "$data/icons/hicolor/icon-theme.cache" ] && command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f "$data/icons/hicolor" || true
fi

echo "Hill Rush removed. Your progress is still in $data/hillrush (delete it to start over)."
