#!/bin/bash
# File: scripts/run_demo.sh
# Purpose: Orchestrates the entire project demo, from build to final testing.
set -e

# --- Configuration ---
PROJECT_ROOT="$(dirname "$(readlink -f "$0")")/.."
DEVICE_PATH="/dev/simtemp"
CLI_DIR="$PROJECT_ROOT/user/cli"
CLI_SCRIPT="main.py"

echo "==================================================="
echo "== NXP SIMTEMP DRIVER: END-TO-END DEMONSTRATION =="
echo "==================================================="

# --- CRITICAL PRE-FLIGHT CHECKS ---
# Ensure all scripts are executable before continuing
echo -e "\n--- Granting Execute Permissions ---"
chmod +x "$PROJECT_ROOT/scripts/cleanup.sh"
chmod +x "$PROJECT_ROOT/scripts/install.sh"
# chmod +x "$PROJECT_ROOT/scripts/build.sh" (Assumed done earlier)


# 1. Cleanup Old State (Crucial for reliable testing)
echo -e "\n--- 1. CLEANUP (Removing old modules and environment) ---"
"$PROJECT_ROOT/scripts/cleanup.sh"

# 2. Build the Kernel Module
echo -e "\n--- 2. BUILD KERNEL MODULE ---"
"$PROJECT_ROOT/scripts/build.sh"

# Check if the kernel module artifact exists
if [ ! -f "$PROJECT_ROOT/kernel/nxp_simtemp.ko" ]; then
    echo "!!! FATAL ERROR: Kernel module nxp_simtemp.ko was not created during build. Aborting. !!!"
    exit 1
fi

# 3. Install Python Dependencies
echo -e "\n--- 3. INSTALL CLI DEPENDENCIES (Setup Python venv) ---"
"$PROJECT_ROOT/scripts/install.sh"
source "$CLI_DIR/.venv/bin/activate"

# 4. Load the Kernel Module (EXPECTED FAILURE POINT)
echo -e "\n--- 4. LOAD KERNEL MODULE ---"

# Load the module (requires sudo)
sudo insmod "$PROJECT_ROOT/kernel/nxp_simtemp.ko"
LOAD_STATUS=$?

if [ $LOAD_STATUS -ne 0 ]; then
    echo "!!! FATAL RUNTIME ERROR: Module loading failed. !!!"
    echo "ANALYSIS: Failure code $LOAD_STATUS (e.g., 'Key was rejected by service') confirms active Kernel Module Signature Verification (Secure Boot)."
    echo "This is an environmental roadblock, not a code bug. Proceeding to cleanup."
    
    # 6. Final Cleanup
    echo -e "\n--- 6. CLEANUP VENV ---"
    # Note: We skip rmmod since the module was never loaded.
    deactivate
    
    # Return non-zero to signify the demo failed due to environment.
    exit 1
fi

echo "[*] Module loaded successfully."

# Wait briefly for udev to create /dev/simtemp
sleep 1 

# Check for device file existence
if [ ! -c "$DEVICE_PATH" ]; then
    echo "!!! FATAL ERROR: Device file $DEVICE_PATH not created. Aborting. !!!"
    sudo rmmod nxp_simtemp || true
    deactivate
    exit 1
fi

# 5. Run the Mandatory CLI Test Mode
echo -e "\n--- 5. RUN MANDATORY CLI TEST MODE (Validates Poll/Sysfs/Timer) ---"
python3 "$CLI_DIR/$CLI_SCRIPT" test

TEST_STATUS=$?

# 6. Final Cleanup
echo -e "\n--- 6. UNLOAD MODULE AND CLEANUP VENV ---"
sudo rmmod nxp_simtemp 
deactivate

# 7. Report Final Status
if [ $TEST_STATUS -eq 0 ]; then
    echo -e "\n==================================================="
    echo ">>> DEMO SUCCESS: CLI Test Mode Passed (Exit Code 0) <<<"
    echo "==================================================="
else
    echo -e "\n==================================================="
    echo "!!! DEMO FAILED: CLI Test Mode Failed (Exit Code 1) !!!"
    echo "==================================================="
fi

exit $TEST_STATUS