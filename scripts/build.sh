#!/bin/bash
# File: scripts/build.sh (FINAL AGGRESSIVE OVERRIDE)
set -e

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "--- Building native x86_64 kernel module with aggressive CFLAGS override ---"

unset ARCH
unset CROSS_COMPILE

KERNEL_SRC=/lib/modules/$(uname -r)/build

if [ ! -d "$KERNEL_SRC" ]; then
    echo "ERROR: Kernel headers directory not found at '$KERNEL_SRC'. Please ensure headers are installed."
    exit 1
fi

# Clean previous build artifacts
make -C "$KERNEL_SRC" M="$PROJECT_ROOT/kernel" clean

# FINAL AGGRESSIVE OVERRIDE:
# We're trying to force KBUILD_CFLAGS to be an empty string, or contain only allowed flags.
# The 'override' keyword is a make internal, harder to use from command line.
# Instead, we pass a custom CFLAGS to make, which might take precedence for external modules.
# We explicitly remove the problematic flag, hoping 'make' will respect this.

# Let's try this specific override, which should clear KBUILD_CFLAGS and then add our safe ones.
make -C "$KERNEL_SRC" M="$PROJECT_ROOT/kernel" \
     V=1 \
     KBUILD_CFLAGS_MODULE="-Wno-error -Wno-unused-variable -Wno-declaration-after-statement -Wno-pointer-sign -Wno-address-of-packed-member -fno-stack-protector -fno-sanitize=address -fno-sanitize=bounds-strict -fno-sanitize=shift -fno-sanitize=bool -fno-sanitize=enum" \
     modules

echo "Build attempt complete. Check output for 'nxp_simtemp.ko'."