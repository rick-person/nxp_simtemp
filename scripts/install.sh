#!/bin/bash
# File: scripts/install.sh
# Purpose: Sets up the Python virtual environment and installs dependencies.

set -e

PROJECT_ROOT="$(dirname "$(readlink -f "$0")")/.."
CLI_DIR="$PROJECT_ROOT/user/cli"
VENV_PATH="$CLI_DIR/.venv"
REQUIREMENTS="$CLI_DIR/requirements.txt"

echo "[*] Setting up Python virtual environment..."

# 1. Create virtual environment if it doesn't exist
if [ ! -d "$VENV_PATH" ]; then
    python3 -m venv "$VENV_PATH"
    echo "[*] Virtual environment created at $VENV_PATH"
fi

# 2. Activate Venv (for the script execution)
source "$VENV_PATH/bin/activate"

# 3. Install dependencies from requirements.txt (if file exists)
if [ -f "$REQUIREMENTS" ]; then
    pip install --upgrade pip
    pip install -r "$REQUIREMENTS"
    echo "[*] Python dependencies installed."
else
    echo "[*] Note: requirements.txt not found. Assuming only standard libraries are used."
fi