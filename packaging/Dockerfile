# Build environment for portable Linux binaries.
# AlmaLinux 8 has glibc 2.28, so binaries built here run on Ubuntu 20.04+,
# Debian 10+, Fedora, Arch, openSUSE and other current desktop distros.
FROM almalinux:8

RUN dnf -y install \
        gcc-toolset-13-gcc gcc-toolset-13-gcc-c++ gcc-toolset-13-binutils \
        cmake make file findutils \
        libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel \
        libXi-devel libXext-devel libXrender-devel mesa-libGL-devel \
    && dnf clean all

ENV PATH=/opt/rh/gcc-toolset-13/root/usr/bin:$PATH \
    APPIMAGE_EXTRACT_AND_RUN=1
