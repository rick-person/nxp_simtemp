// File: kernel/nxp_simtemp.c

struct nxp_simtemp_dev{
    struct device *dev;         //Pointer to device structure (logging, sysfs)
    struct miscdevice mis_dev;  //Character device structure for /dev/simtemp
    struct resource *mem_res;   //Virtual memory from DT 'reg'
    
    //  Configuration (see DT/Sysfs)
    u32 sampling_ms;    //Sample period (see DT/sysfs 'sampling-ms')
    s32 threshold_mC;   //Sample period (see DT/sysfs 'threshold-ms')

    //Core state and locking
    int mode;   //simulation mode (normal/noisy/ramp)
    u32 status_flags    //Status flags (see Flags for the flag (God))

    spinlock_t lock;    //shared state buffer protection lock
    wait_queue_head_t read_queue; //Poll/read blocking queue
};

static int nxp_simtemp_probe(struct platform_device *pdev){
    //allocate the 'struct nxp_simtemp_dev'
    // read the ST properties
    dev_info(&pdev->dev, "NXP SimTemp driver probed succesfully.\n");
    return 0;
}

static int nxp_simtemp_remove(struct platform_device *pdev){
    //cancel timer. free memory, unregister devices
    dev_info(&pdev->dev, "NXP SimTemp driver removed succesfully.\n");
    return 0;
}

static struct platform_driver nxp_simtemp_driver = {
    .probe = nxp_simtemp_probe,
    .remove = nxp_simtemp_remove,
    .driver = {
        .name = "nxp_simtemp", //name used by platform bus
        .of_match_table = nxp_simtemp_of_match,
    },
};

//module init and exit
static int __init nxp_simtemp_init(void){
    return platform_driver_register(&nxp_simtemp_driver);
}

static void __exit nxp_simtemp_exit(void){
    platform_driver_unregister(&nxp_simtemp_driver);
}

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);  

//metadata
MODULE_LICENSE("GPL");
MODULE_LICENSE("Ricardo Loya");
MODULE_DESCRIPTION("NXP SimTemp sensor platform driver.");

