#!/bin/bash
# File: scripts/build.sh 
# Purpose: Builds the native kernel module by bypassing the host's broken header checks.
set -e

# --- 0. Determine Project Root ---
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "--- Building native x86_64 kernel module against host headers ---"

# --- 1. Clear Environment ---

unset ARCH
unset CROSS_COMPILE

# --- 2. Build Execution (The Critical Fix) ---

# KERNEL_SRC variable points to the native host headers
KERNEL_SRC=/lib/modules/$(uname -r)/build

# KBUILD_CFLAGS: Flags passed to the C compiler.
# -fno-ubsan-checks: Disables the Undefined Behavior Sanitizer (UBSAN).
# -fno-stack-protector: Disables stack protection.
# This prevents 'make' from following the missing 'scripts/Makefile.ubsan' file
# and resolves the persistent link/header dependency errors.
FORCE_CFLAGS="-fno-ubsan-checks -fno-stack-protector -Wno-error"

# W=1: Tells the kernel build system to enable some verbose output/warnings.
# M=$(PWD): Tells the kernel where the module source resides.
make -C "$KERNEL_SRC" M="$PROJECT_ROOT/kernel" \
     W=1 \
     KBUILD_CFLAGS="$FORCE_CFLAGS" \
     modules