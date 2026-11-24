# Cross-compiler for 64-bit Windows builds (MinGW-w64, posix threads).
FROM ubuntu:24.04
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64 \
        mingw-w64-x86-64-dev cmake ninja-build \
    && rm -rf /var/lib/apt/lists/*
