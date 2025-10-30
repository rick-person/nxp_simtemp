# File: scripts/build.sh (FINAL Portable & Robust Fix)

#!/bin/bash
# Exit immediately if a command exits with a non-zero status.
set -e

# --- 0. Determine Project Root (Portable) ---
# Use 'dirname' and 'readlink -f' to find the absolute path of the script itself,
# then move up one directory level to the project root.
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# --- 1. Locate and Set Toolchain Path ---

# Toolchain root is always relative to the Project Root:
TOOLCHAIN_ROOT="$PROJECT_ROOT/nxp_toolchain"

# Dynamically find the specific toolchain folder inside the root
# Note: We use find on the determined toolchain directory.
TOOLCHAIN_DIR=$(find "$TOOLCHAIN_ROOT" -maxdepth 1 -type d -name "arm-gnu-toolchain-*" | head -n 1)

if [ -z "$TOOLCHAIN_DIR" ]; then
    echo "ERROR: Toolchain directory not found in $TOOLCHAIN_ROOT. Check extraction."
    exit 1
fi

# The actual binaries are in the 'bin' subdirectory
TOOLCHAIN_BIN="$TOOLCHAIN_DIR/bin"

# Set necessary environment variables
export ARCH=arm
export CROSS_COMPILE=arm-none-linux-gnueabihf- 

# --- 2. Build Kernel Module ---

# KERNEL_SRC variable should point to the detected headers
KERNEL_SRC=/lib/modules/$(uname -r)/build 

echo "Building kernel module for ARMv7 using KERNEL_SRC=$KERNEL_SRC"
echo "Cross-Compiler Path: $TOOLCHAIN_BIN (Calculated Portably)"
echo "---"

# CRITICAL FIX: Explicitly set the PATH environment variable for 'make' execution.
/usr/bin/env PATH="$TOOLCHAIN_BIN:$PATH" make -C "$KERNEL_SRC" M="$PROJECT_ROOT/kernel" modules