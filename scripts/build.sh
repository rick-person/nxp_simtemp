#!/bin/bash
# File: scripts/build.sh (Native Host Compilation)
# Purpose: Builds the kernel module against the host's native headers.
set -e

# --- 0. Determine Project Root (Portable) ---
# Finds the project root, even if the script is called from another location.
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "--- Building native x86_64 kernel module against host headers ---"

# --- 1. Clear Cross-Compile Variables (CRITICAL for Native Build) ---
# Ensures no lingering cross-compiler settings interfere.
unset ARCH
unset CROSS_COMPILE

# --- 2. Build Execution ---
# KERNEL_SRC variable points to the native host headers
KERNEL_SRC=/lib/modules/$(uname -r)/build

# Run the native kernel's external module build system
make -C "$KERNEL_SRC" M="$PROJECT_ROOT/kernel" modules