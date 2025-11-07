# NXP Simulated Temperature Sensor (nxp_simtemp)

## 📝 Project Summary
This project implements the "Virtual Sensor + Alert Path" challenge. It consists of a **Linux kernel platform driver** (`nxp_simtemp`) that simulates periodic temperature sampling and a user-space **Python CLI application** for configuration and real-time alert monitoring via `poll()`/`epoll()` on a character device (`/dev/simtemp`).

---

## 🔗 Submission Details

**REQUIRED ACTION:** Use `git-send email` to send the patch to this file, ensuring you replace the placeholders below with your actual links.

| Detail | Link |
| :--- | :--- |
| **Git Repository** | [LINK_TO_YOUR_GIT_REPO_HERE] |
| **Demo Video** | [LINK_TO_YOUR_VIDEO_DEMO_HERE] |

---

## ✅ Acceptance Criteria Status

| Requirement | Status | Notes |
| :--- | :--- | :--- |
| **1. Build & Load** | **SUCCESS** | Module compiles cleanly using `scripts/build.sh`. |
| **2. Data Path (Poll/Read)** | **IMPLEMENTED** | Supports blocking reads (via `poll`/`epoll` in userspace) and correctly signals **POLLPRI** on threshold crossing. |
| **3. Config Path (Sysfs)** | **IMPLEMENTED** | `sampling_ms`, `threshold_mC`, `mode`, and `stats` attributes are fully supported. |
| **4. Robustness** | **IMPLEMENTED** | Clean teardown via `rmmod` and proper locking/error checking are implemented. |
| **5. User App** | **IMPLEMENTED** | CLI test mode (via `main.py test`) is fully coded and ready to validate alerts. |
| **6. Docs & Git** | **COMPLETE** | `README.md`, `DESIGN.md`, and `AI_NOTES.md` provided. |

---

## 🚀 Execution Guide

### Prerequisites
* **Environment:** Ubuntu/Debian environment (Tested on Ubuntu 22.04 LTS).
* **Dependencies:** Matching kernel headers (`linux-headers-$(uname -r)`), `python3`, and `venv` installed.
* **Permissions:** Required file permissions (`chmod +x scripts/*.sh`).

### 1. Build & Setup
Execute the primary build script, which handles kernel module compilation, Python environment setup, and dependency installation.

```bash
# Set execute permissions for all scripts
chmod +x scripts/*.sh

# Run the build process
scripts/build.sh