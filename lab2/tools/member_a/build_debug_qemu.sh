#!/usr/bin/env bash
# Build a separate, uninstalled QEMU for source-level debugging.
set -euo pipefail
lab_dir="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
mkdir -p "$lab_dir/_local"
cd "$lab_dir/_local"
archive=qemu-4.1.1.tar.xz
if [[ ! -f "$archive" ]]; then
    curl --fail --location --retry 3 https://download.qemu.org/qemu-4.1.1.tar.xz -o "$archive"
fi
if [[ ! -f qemu-4.1.1/configure ]]; then
    tar -xJf "$archive"
fi
mkdir -p qemu-build
cd qemu-build
../qemu-4.1.1/configure --target-list=riscv64-softmmu --enable-debug \
    --disable-werror --disable-docs --disable-gtk --disable-sdl \
    --disable-capstone --disable-slirp > configure.log 2>&1
make -j"${LAB2_JOBS:-8}" > build.log 2>&1
./riscv64-softmmu/qemu-system-riscv64 --version
