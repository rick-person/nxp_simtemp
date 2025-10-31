// File: kernel/nxp_simtemp.h (New Header File for Core Definitions)

#include <linux/types.h>

// Max number of samples the buffer can hold (e.g., 2 seconds of 100ms samples)
#define SIMTEMP_MAX_SAMPLES     20
#define SIMTEMP_BUFFER_SIZE     (sizeof(struct simtemp_sample) * SIMTEMP_MAX_SAMPLES)

// The mandatory binary data record format
struct simtemp_sample {
    __u64 timestamp_ns;      // Monotonic timestamp
    __s32 temp_mC;           // milli-degree Celsius
    __u32 flags;             // bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED
} __attribute__((packed));

// The Ring Buffer Structure
struct simtemp_ring_buffer {
    // The buffer array
    struct simtemp_sample buf[SIMTEMP_MAX_SAMPLES]; 
    
    // Indices for managing the buffer
    unsigned int head;      // Index where the producer (timer) writes
    unsigned int tail;      // Index where the consumer (read()) reads
};