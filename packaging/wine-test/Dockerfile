# Runs the Windows build under Wine with a virtual display, for smoke tests.
# (Recommended packages are needed: they bring the X11 libraries Wine's
# display driver loads at runtime.)
FROM ubuntu:24.04
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y \
        wine64 xvfb xauth libgl1-mesa-dri libglx-mesa0 \
    && rm -rf /var/lib/apt/lists/*
ENV WINEDEBUG=-all WINEPREFIX=/tmp/wineprefix
