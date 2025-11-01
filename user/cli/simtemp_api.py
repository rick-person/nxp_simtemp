# File: user/cli/simtemp_api.py

import struct
from datetime import datetime, timezone
from select import select

# --- Binary Data Structure ---
# Based on struct simtemp_sample: 
# __u64 timestamp_ns (8 bytes)
# __s32 temp_mC (4 bytes)
# __u32 flags (4 bytes)
SAMPLE_STRUCT_FORMAT = '<Qii' # < = Little-endian; Q=u64; i=s32; I=u32
SAMPLE_SIZE = struct.calcsize(SAMPLE_STRUCT_FORMAT)

# Flags (must match SIMTEMP_FLAG_NEW_SAMPLE=1 and SIMTEMP_FLAG_THRESHOLD_CROSSED=2)
NEW_SAMPLE_FLAG = 0x1
THRESHOLD_CROSSED_FLAG = 0x2

class SimtempSample:
    def __init__(self, raw_data):
        # Unpack the binary data
        self.timestamp_ns, self.temp_mC, self.flags = struct.unpack(
            SAMPLE_STRUCT_FORMAT, raw_data
        )
        self.temp_C = self.temp_mC / 1000.0
        self.is_alert = bool(self.flags & THRESHOLD_CROSSED_FLAG)

    def to_line(self):
        # Formats the required output line (Acceptance Criteria 2.2)
        
        # Convert nanoseconds to a more readable datetime (using UTC)
        # Note: Linux monotonic time is hard to convert precisely to wall time, 
        # but this conversion provides the required format.
        timestamp_s = self.timestamp_ns / 1_000_000_000.0
        dt_obj = datetime.fromtimestamp(timestamp_s, tz=timezone.utc)
        
        # Format: 2025-09-22T20:15:04.123Z temp=44.1C alert=0/1
        return f"{dt_obj.isoformat(timespec='milliseconds').replace('+00:00', 'Z')} " \
               f"temp={self.temp_C:.1f}C alert={int(self.is_alert)}"

def set_sysfs_value(attribute_name, value):
    """Writes a value to a specified Sysfs attribute file."""
    SYSFS_PATH_BASE = '/sys/class/misc/simtemp/' # Assumed common path for miscdevice
    try:
        with open(os.path.join(SYSFS_PATH_BASE, attribute_name), 'w') as f:
            f.write(str(value))
        print(f"[*] Sysfs: {attribute_name} set to {value}")
    except FileNotFoundError:
        print(f"ERROR: Sysfs path {SYSFS_PATH_BASE} not found. Module loaded correctly?")
        sys.exit(1)
    except PermissionError:
        print(f"ERROR: Permission denied when writing to Sysfs. Run as root/sudo?")
        sys.exit(1)