#!/usr/bin/env bash
set -euo pipefail

# Run QEMU for this project. If qemu-system-riscv64 is missing,
# try to fetch a local copy into ../tools using apt-get download.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Create ../tools per request
pushd "$SCRIPT_DIR/.." >/dev/null
TOOLS_DIR="$(pwd)/tools"
mkdir -p "$TOOLS_DIR"
popd >/dev/null

QEMU_BIN=""

find_qemu() {
    if command -v qemu-system-riscv64 >/dev/null 2>&1; then
        QEMU_BIN="$(command -v qemu-system-riscv64)"
        return
    fi
    if [ -x "$TOOLS_DIR/qemu-system-riscv64" ]; then
        QEMU_BIN="$TOOLS_DIR/qemu-system-riscv64"
        return
    fi
}

download_qemu() {
    echo "qemu-system-riscv64 not found; attempting local download to $TOOLS_DIR"
    if ! command -v apt-get >/dev/null 2>&1; then
        echo "apt-get not available; please install qemu-system-misc manually."
        exit 1
    fi
    pushd "$TOOLS_DIR" >/dev/null
    # Best-effort package download + extract
    apt-get update
    apt-get download qemu-system-misc
    deb="$(ls -1 qemu-system-misc_*.deb | head -n1 || true)"
    if [ -z "$deb" ]; then
        echo "Failed to download qemu-system-misc package."
        exit 1
    fi
    rm -rf qemu-extract
    mkdir -p qemu-extract
    dpkg -x "$deb" qemu-extract
    if [ ! -x qemu-extract/usr/bin/qemu-system-riscv64 ]; then
        echo "Extracted package missing qemu-system-riscv64."
        exit 1
    fi
    cp qemu-extract/usr/bin/qemu-system-riscv64 "$TOOLS_DIR/"
    chmod +x "$TOOLS_DIR/qemu-system-riscv64"
    rm -rf "$deb" qemu-extract
    popd >/dev/null
    QEMU_BIN="$TOOLS_DIR/qemu-system-riscv64"
}

ensure_kernel() {
    if [ ! -f "$SCRIPT_DIR/kernel.bin" ]; then
        echo "kernel.bin not found; building via make..."
        make -C "$SCRIPT_DIR"
    fi
}

main() {
    find_qemu
    if [ -z "$QEMU_BIN" ]; then
        download_qemu
    fi

    ensure_kernel

    echo "Using QEMU: $QEMU_BIN"
    exec "$QEMU_BIN" -machine virt -nographic -m 128M -kernel "$SCRIPT_DIR/kernel.bin"
}

main "$@"
