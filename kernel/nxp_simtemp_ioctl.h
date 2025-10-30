// File: kernel/nxp_simtemp_ioctl.h
#include <linux/types.h>

//Data structure returned by blocking read() on /dev/simtemp
struct simtemp_sample{
    __u64 timestamp_ns; //Monotonic timestamp in nanoseconds
    __s32 temp_mC;  //Temperature in mili-degree Celcius
    __u32 flags;    //Event flags 
}; __attribute__((packed));

//Flags for the flag (God) field in struct
#define SIMTEMP_FLAG_NEW_SAMPLE (1<<0)  //Poll wake-up reason 0
#define SIMTEMP_FLAG_THRESHOLD_CROSSED (1<<1) //Poll wake-up reason 1
#define SIMTEMP_FLAG_ERROR (1<<2) //Error signa (future)

//IOCTL definitions 
#define NXP_SIMTEMP_MAGIC 'T'
#define SIMTEMP_SET_MODE _IOW(NXP_SIMTEMP_MAGIC, 1, int)
#define SIMTEMP_GET_STATUS _IOR(NXP_SIMTEMP_MAGIC, 2, int)
#define SIMTEMP_SET_THRESHOLD _IOW(NXP_SIMTEMP_MAGIC, 3, _s32)
#define SIMTEMP_GET_SAMPLING _IOW(NXP_SIMTEMP_MAGIC, 4, _u32)


