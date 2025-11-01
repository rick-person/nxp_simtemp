#!/bin/bash
# File: scripts/install.sh
# Purpose: Creates a Python virtual environment and installs CLI dependencies.
set -e

echo "--- Installing CLI Dependencies ---"

# 1. Ensure Python 3 is available
if ! command -v python3 &> /dev/null
then
    echo "ERROR: python3 could not be found. Please install a compatible version."
    exit 1
fi

# 2. Navigate to CLI directory
cd user/cli/

# 3. Create the Virtual Environment (.venv)
if [ ! -d ".venv" ]; then
    echo "[*] Creating virtual environment..."
    # Use the default python3 (which is usually 3.10+ on 22.04)
    python3 -m venv .venv
fi

# 4. Activate Venv and Install Requirements
echo "[*] Installing requirements from requirements.txt..."
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt

echo "[*] Installation complete. Environment ready."
deactivate # Deactivate the venv