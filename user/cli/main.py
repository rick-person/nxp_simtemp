# File: user/cli/main.py

import os
import sys
import time
import select
import struct
import argparse

# --- 1. Constants and Helpers (Mocking API for standalone execution) ---

# Define the constants/structs for the binary interface (These should match kernel headers)
SAMPLE_SIZE = 12  # u64 (timestamp) + s32 (temp)
NXP_SIMTEMP_MAGIC = 0xA5

# IOCTL Command Definitions (Matching nxp_simtemp_ioctl.h)
# We assume the user has a local copy of ioctl codes or they are mocked here.
SIMTEMP_SET_THRESHOLD = (0x01 | (NXP_SIMTEMP_MAGIC << 8))
SIMTEMP_SET_SAMPLING = (0x02 | (NXP_SIMTEMP_MAGIC << 8))

# Assume existence of fcntl.ioctl for real IOCTL calls
try:
    import fcntl
except ImportError:
    print("Warning: fcntl not available. IOCTL configuration will be mocked.")
    fcntl = None


class SimtempSample:
    """Represents the binary record read from the device."""
    def __init__(self, raw_data):
        # Format: < (little-endian), Q (u64 timestamp), i (s32 temp), I (u32 flags)
        if len(raw_data) != SAMPLE_SIZE:
             raise ValueError("Raw data size mismatch for SimtempSample.")
             
        self.timestamp_ns, self.temp_mC, self.flags = struct.unpack('<QiI', raw_data)
        
        self.temp_C = self.temp_mC / 1000.0

    def to_line(self):
        """Formats the sample output as required by the challenge."""
        from datetime import datetime
        
        # Convert nanoseconds to seconds and microseconds for datetime object
        ts_s = self.timestamp_ns / 1_000_000_000
        
        # Format timestamp: YYYY-MM-DDT...Z (ISO 8601)
        # We approximate the millisecond part
        dt_obj = datetime.fromtimestamp(ts_s)
        timestamp_str = dt_obj.strftime('%Y-%m-%dT%H:%M:%S') + f'.{int((ts_s * 1000) % 1000):03}Z'
        
        alert = 1 if self.flags & 0x02 else 0 # Bit 1 is THRESHOLD_CROSSED
        
        return f"{timestamp_str} temp={self.temp_C:.1f}C alert={alert}"

def set_sysfs_value(attribute, value):
    """Writes a value to the specified Sysfs attribute file."""
    path = os.path.join(SYSFS_PATH_BASE, attribute)
    try:
        with open(path, 'w') as f:
            f.write(str(value) + '\n')
        print(f"[*] Sysfs: Set {attribute} = {value}")
    except FileNotFoundError:
        print(f"ERROR: Sysfs file not found at {path}. Check device registration.")
    except Exception as e:
        print(f"ERROR: Failed to write to {path}: {e}")

def ioctl_set_config(fd, cmd, value):
    """Performs an IOCTL call to set a configuration value."""
    if fcntl:
        try:
            # IOCTL commands in Linux typically expect the argument (value) as an int/long
            fcntl.ioctl(fd, cmd, struct.pack('i', value)) 
            print(f"[*] IOCTL: Executed command {cmd:#x} with value {value}")
        except Exception as e:
            print(f"ERROR: IOCTL failed for command {cmd:#x}: {e}")
    else:
        print(f"WARNING: IOCTL method called but fcntl module is missing. (Cmd: {cmd:#x}, Value: {value})")


DEVICE_PATH = '/dev/simtemp'
SYSFS_PATH_BASE = '/sys/class/misc/simtemp/'

# --- 2. SimtempCLI Class ---

class SimtempCLI:
    def __init__(self):
        # The file is opened O_RDONLY, but the fd is used for IOCTL config too.
        try:
            self.device_fd = os.open(DEVICE_PATH, os.O_RDONLY | os.O_NONBLOCK) 
            print(f"[*] Device opened successfully at {DEVICE_PATH}")
        except FileNotFoundError:
            print(f"ERROR: Device file not found at {DEVICE_PATH}. Is the kernel module loaded?")
            sys.exit(1)
        except Exception as e:
            print(f"Error opening device: {e}")
            sys.exit(1)

    def read_samples(self, count=None):
        """Continuously reads and prints samples from the kernel driver."""
        print("Reading samples. Press Ctrl+C to stop.")
        
        read_count = 0
        while count is None or read_count < count:
            try:
                # Use poll to ensure data is ready (more robust than looping)
                poller = select.poll()
                poller.register(self.device_fd, select.POLLIN)
                
                # Wait for up to 5 seconds for a new sample
                events = poller.poll(5000)
                
                if events:
                    raw_data = os.read(self.device_fd, SAMPLE_SIZE)
                    if raw_data:
                        sample = SimtempSample(raw_data)
                        print(sample.to_line())
                        read_count += 1
                else:
                    # Timeout reached
                    print("[*] Waiting for data (Timeout).")
                
            except KeyboardInterrupt:
                print("\n[!] Exiting reader loop.")
                break
            except Exception as e:
                print(f"Error reading from device: {e}")
                break

    def monitor_events(self):
            """Monitors device for new samples (POLLIN) and threshold alerts (POLLPRI)."""
            print(f"[*] Monitoring events on {DEVICE_PATH}. Press Ctrl+C to stop.")
            
            # 1. Setup Poll Object
            poller = select.poll()
            
            POLL_FLAGS = select.POLLIN | select.POLLPRI | select.POLLERR | select.POLLHUP
            poller.register(self.device_fd, POLL_FLAGS)
            
            # 2. Event Loop
            try:
                while True:
                    # Wait indefinitely (timeout=-1) for events
                    events = poller.poll(timeout=-1) 
                    
                    for fd, event in events:
                        if fd == self.device_fd:
                            # Check for the alert flag first
                            if event & select.POLLPRI:
                                print("\n\n>>> !!! THRESHOLD ALERT CROSSED (POLLPRI) !!! <<<\n")
                            
                            # Data is ready to read (POLLIN)
                            if event & select.POLLIN:
                                # Keep reading until the buffer is clear
                                while True:
                                    try:
                                        raw_data = os.read(self.device_fd, SAMPLE_SIZE)
                                        if not raw_data:
                                            break
                                        sample = SimtempSample(raw_data)
                                        print(sample.to_line())
                                    except BlockingIOError:
                                        break
                                    except Exception:
                                        break

                            if event & (select.POLLERR | select.POLLHUP):
                                print(f"\n[!] Device error or hangup (Event: {event}). Exiting.")
                                return
            except KeyboardInterrupt:
                print("\n[!] Exiting event monitor.")
            
            poller.unregister(self.device_fd)

    def close(self):
        os.close(self.device_fd)
        print("[*] Device closed.")

    def set_config(self, threshold_mC=None, sampling_ms=None, use_ioctl=False):
        """Sets configuration using Sysfs or IOCTL."""
        fd = self.device_fd
        
        if use_ioctl:
            if threshold_mC is not None:
                ioctl_set_config(fd, SIMTEMP_SET_THRESHOLD, threshold_mC)
            if sampling_ms is not None:
                ioctl_set_config(fd, SIMTEMP_SET_SAMPLING, sampling_ms)
        else:
            # Use Sysfs as the default configuration method
            if threshold_mC is not None:
                set_sysfs_value('threshold_mC', threshold_mC)
            if sampling_ms is not None:
                set_sysfs_value('sampling_ms', sampling_ms)

        # Note: We reset the ring buffer head/tail here, though the kernel typically handles this 
        # on open/close or probe/remove. No specific IOCTL for reset is defined, so we rely on the kernel.

    def run_test_mode(self, use_ioctl=False):
            """Sets a low threshold and verifies a THRESHOLD_CROSSED event occurs."""
            TEST_THRESHOLD_MILI_C = 20000 # 20.0 C 
            TEST_TIMEOUT_MS = 5000 # 5 seconds
            
            print(f"\n--- Starting CLI Test Mode (IOCTL={use_ioctl}) ---")
            
            # 1. Configure Threshold 
            self.set_config(threshold_mC=TEST_THRESHOLD_MILI_C, use_ioctl=use_ioctl)
            
            # 2. Setup Poll for Alert
            poller = select.poll()
            
            # We only register for the alert event (POLLPRI)
            POLL_ALERT_FLAGS = select.POLLPRI | select.POLLERR | select.POLLHUP
            poller.register(self.device_fd, POLL_ALERT_FLAGS)
            
            print(f"[*] Waiting for THRESHOLD ALERT (<{TEST_THRESHOLD_MILI_C} mC) for {TEST_TIMEOUT_MS/1000}s...")
            
            # 3. Wait for Event
            # The test assumes the simulated temperature is > 20C, so an alert should happen quickly.
            events = poller.poll(timeout=TEST_TIMEOUT_MS) 
            
            poller.unregister(self.device_fd)

            # 4. Analyze Results
            if events and events[0][1] & select.POLLPRI:
                # Read the sample to clear the event queue (important for continuous polling)
                # Since we only polled for POLLPRI, we must read here to ensure the alert is consumed.
                try:
                    raw_data = os.read(self.device_fd, SAMPLE_SIZE)
                    if raw_data:
                        sample = SimtempSample(raw_data)
                        print(f"[*] Sample read to clear buffer: {sample.to_line()}")
                except Exception:
                    # Ignore read failure, the primary goal is event detection
                    pass

                print("\n>>> TEST PASSED: THRESHOLD ALERT RECEIVED! The full pipeline is validated. <<<")
                return 0
            else:
                print(f"\n!!! TEST FAILED: Timeout reached or wrong event received. Pipeline broken. !!!")
                # Must exit non-zero on failure
                sys.exit(1)


# --- 3. Command Line Interface Logic ---

def main():
    parser = argparse.ArgumentParser(description="NXP Simulated Temperature Sensor CLI.")
    
    # Subcommand structure
    subparsers = parser.add_subparsers(dest='command', required=True)

    # READ command
    parser_read = subparsers.add_parser('read', help='Read a number of samples.')
    parser_read.add_argument('--count', type=int, default=5, help='Number of samples to read.')

    # MONITOR command
    subparsers.add_parser('monitor', help='Continuously monitor events and samples.')

    # CONFIG command
    parser_config = subparsers.add_parser('config', help='Set device configuration.')
    parser_config.add_argument('--threshold', type=int, help='Alert threshold in milli-Celsius.')
    parser_config.add_argument('--sampling-ms', type=int, help='Sampling period in milliseconds.')
    parser_config.add_argument('--ioctl', action='store_true', help='Use ioctl for configuration (default is sysfs).')

    # TEST command (Used by run_demo.sh)
    parser_test = subparsers.add_parser('