# File: user/cli/main.py

import os
import sys
import time
import select

from simtemp_api import SimtempSample, SAMPLE_SIZE

DEVICE_PATH = '/dev/simtemp'
SYSFS_PATH_BASE = '/sys/class/misc/simtemp/' # Assumed path for Sysfs access

class SimtempCLI:
    def __init__(self):
        try:
            # Open the character device file
            self.device_fd = os.open(DEVICE_PATH, os.O_RDONLY | os.O_NONBLOCK) # Use non-blocking for initial check
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
                # Set the file descriptor to blocking mode for continuous reading (for poll in next sprint)
                # For now, we'll try a simple read loop.
                raw_data = os.read(self.device_fd, SAMPLE_SIZE)
                
                if raw_data:
                    sample = SimtempSample(raw_data)
                    print(sample.to_line())
                    read_count += 1
                else:
                    # If non-blocking and no data, wait briefly
                    time.sleep(0.01)

            except BlockingIOError:
                # This handles O_NONBLOCK if used; poll is better (Sprint 8)
                time.sleep(0.01)
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
            
            # Register the device file descriptor (self.device_fd) for reading (POLLIN) 
            # and urgent data (POLLPRI, used for our threshold alert).
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
                                raw_data = os.read(self.device_fd, SAMPLE_SIZE)
                                if raw_data:
                                    sample = SimtempSample(raw_data)
                                    print(sample.to_line())

                            if event & (select.POLLERR | select.POLLHUP):
                                print(f"\n[!] Device error or hangup (Event: {event}). Exiting.")
                                return
            except KeyboardInterrupt:
                print("\n[!] Exiting event monitor.")
            
            # Unregister the file descriptor
            poller.unregister(self.device_fd)

    def close(self):
        os.close(self.device_fd)
        print("[*] Device closed.")

    def run_test_mode(self):
            """Sets a low threshold and verifies a THRESHOLD_CROSSED event occurs."""
            TEST_THRESHOLD_MILI_C = 20000 # 20.0 C (Assuming simulated temp is higher than 25C default)
            TEST_TIMEOUT_MS = 5000 # 5 seconds
            
            print(f"\n--- Starting CLI Test Mode ---")
            
            # 1. Configure Threshold via Sysfs
            set_sysfs_value('threshold_mC', TEST_THRESHOLD_MILI_C)
            
            # 2. Setup Poll for Alert
            poller = select.poll()
            
            # We only register for the alert event (POLLPRI)
            POLL_ALERT_FLAGS = select.POLLPRI | select.POLLERR | select.POLLHUP
            poller.register(self.device_fd, POLL_ALERT_FLAGS)
            
            print(f"[*] Waiting for THRESHOLD ALERT (<{TEST_THRESHOLD_MILI_C} mC) for {TEST_TIMEOUT_MS/1000}s...")
            
            # 3. Wait for Event
            events = poller.poll(timeout=TEST_TIMEOUT_MS) 
            
            poller.unregister(self.device_fd)

            # 4. Analyze Results
            if events and events[0][1] & select.POLLPRI:
                print("\n>>> TEST PASSED: THRESHOLD ALERT RECEIVED! The full pipeline is validated. <<<")
                return 0
            else:
                print(f"\n!!! TEST FAILED: Timeout reached or wrong event received. Pipeline broken. !!!")
                # Must exit non-zero on failure
                sys.exit(1)

if __name__ == '__main__':
    cli = SimtempCLI()
    try:
        # A simple test run
        cli.read_samples(count=5) 
    finally:
        cli.close()