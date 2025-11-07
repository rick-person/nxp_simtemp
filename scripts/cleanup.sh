#!/bin/bash
# File: scripts/cleanup.sh
# Purpose: Safely removes the kernel module and unloads the Python virtual environment.

set -e

PROJECT_ROOT="$(dirname "$(readlink -f "$0")")/.."
CLI_DIR="$PROJECT_ROOT/user/cli"

echo "[*] Cleaning up environment..."

# 1. Safely remove kernel module if loaded
if lsmod | grep -q "nxp_simtemp"; then
    echo "[*] Unloading nxp_simtemp module..."
    sudo rmmod nxp_simtemp || true
fi

# 2. Deactivate and remove virtual environment
if [ -d "$CLI_DIR/.venv" ]; then
    echo "[*] Removing Python virtual environment..."
    rm -rf "$CLI_DIR/.venv"
fi

echo "[*] Cleanup complete."