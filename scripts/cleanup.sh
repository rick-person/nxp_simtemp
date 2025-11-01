#!/bin/bash
# File: scripts/cleanup.sh
# Purpose: Cleans the project, removing kernel artifacts, module, and venv.

PROJECT_ROOT="$(dirname "$(readlink -f "$0")")/.."
cd "$PROJECT_ROOT"

echo "--- Cleaning Project Artifacts ---"

# 1. Attempt to remove the module first (suppress errors if not loaded)
echo "[*] Attempting to remove nxp_simtemp kernel module..."
sudo rmmod nxp_simtemp 2>/dev/null || true

# 2. Remove all kernel build artifacts (from the Makefile 'clean' target)
echo "[*] Running 'make clean'..."
# Note: We run the clean target against the Makefile, not the build.sh
make -C "$PROJECT_ROOT/kernel" clean

# 3. Remove the Python virtual environment
if [ -d "user/cli/.venv" ]; then
    echo "[*] Removing Python virtual environment..."
    rm -rf user/cli/.venv
fi

# 4. Remove Docker image (optional, but good for a clean slate)
if docker images | grep -q "nxp-simtemp-builder"; then
    echo "[*] Removing Docker image 'nxp-simtemp-builder'..."
    docker rmi nxp-simtemp-builder >/dev/null 2>&1 || true
fi

echo "--- Cleanup complete. Project is back to source state. ---"