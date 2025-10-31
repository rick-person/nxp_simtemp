#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h> 
#include <linux/slab.h> 
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/fs.h>       // For struct file_operations
#include <linux/miscdevice.h> // For miscdevice
#include <linux/io.h>       // For ioremap (though unused, good for platform devices)

// NOTE: This structure is currently missing the ring buffer, which will be added later.
struct nxp_simtemp_dev {
    struct device *dev;              // Pointer to the device structure (for logging, sysfs)
    struct miscdevice misc_dev;      // The character device structure for /dev/simtemp
    struct simtemp_ring_buffer ring_buf;  // <-- NEW: Our bounded ring buffer
    
    // Configuration from DT/Sysfs
    u32 sampling_ms;                 // Sample period
    s32 threshold_mC;                // Alert threshold
    int mode;                        // Simulation mode 
    
    // Core state and locking
    spinlock_t lock;                 // Lock for protecting shared state
    wait_queue_head_t read_queue;    // Queue for blocking reads / poll
};

// --- 2. Character Device File Operations (Dummy for compilation) ---

static int simtemp_open(struct inode *inode, struct file *file) 
{ 
    // Usually increments an open counter and stores sdev in file->private_data
    return 0; 
}
static int simtemp_release(struct inode *inode, struct file *file) 
{ 
    // Usually decrements an open counter
    return 0; 
}
// Read, Write, and Poll operations will be implemented later in Sprint 3.

static const struct file_operations simtemp_fops = {
    .owner      = THIS_MODULE,
    .open       = simtemp_open,
    .release    = simtemp_release,
    // .read, .write, .poll, and .unlocked_ioctl will be added here.
};

// --- 3. Misc Device Registration Structure ---
static struct miscdevice simtemp_misc_dev = {
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "simtemp", // CRITICAL: Creates /dev/simtemp
    .fops   = &simtemp_fops,
};


// --- 4. Platform Driver Probe and Remove (Sub-Task 2.3) ---

static int nxp_simtemp_probe(struct platform_device *pdev)
{
    struct nxp_simtemp_dev *sdev;
    struct device_node *np = pdev->dev.of_node;
    u32 threshold_val;
    int ret;

    dev_info(&pdev->dev, "Starting probe for nxp_simtemp device.\n");

    // 4.1 Allocate and Initialize
    sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
    if (!sdev)
        return -ENOMEM;

    sdev->dev = &pdev->dev;
    spin_lock_init(&sdev->lock);
    init_waitqueue_head(&sdev->read_queue);
    
    // Set initial defaults
    sdev->sampling_ms = 100;
    sdev->threshold_mC = 45000;

    // 4.2 Read DT Properties
    if (np) { // Only attempt to read if a DT node exists
        if (of_property_read_u32(np, "sampling-ms", &sdev->sampling_ms)) {
            dev_warn(sdev->dev, "DT 'sampling-ms' not found, using default: %u ms\n", sdev->sampling_ms);
        }
        if (of_property_read_u32(np, "threshold-mC", &threshold_val) == 0) {
             sdev->threshold_mC = (s32)threshold_val;
        } else {
            dev_warn(sdev->dev, "DT 'threshold-mC' not found, using default: %d mC\n", sdev->threshold_mC);
        }
        if (sdev->sampling_ms == 0) {
            dev_err(sdev->dev, "Invalid sampling period (0 ms) from DT. Aborting.\n");
            return -EINVAL;
        }
    } else {
        dev_info(sdev->dev, "No DT node found, using default parameters.\n");
    }

    // 4.3 Register Misc Device 
    sdev->misc_dev = simtemp_misc_dev; // We need to ensure the .name is set correctly
    sdev->misc_dev.name = "simtemp";   // Ensure the /dev/simtemp name is explicitly set
    sdev->misc_dev.fops = &simtemp_fops; // Use the file_operations struct we defined
    sdev->misc_dev.minor = MISC_DYNAMIC_MINOR;

    ret = misc_register(&sdev->misc_dev);
    
    //error handling
    if (ret) {
        dev_err(sdev->dev, "Failed to register misc device: %d\n", ret);
        return ret;
    }

    // 4.4 Finalize
    platform_set_drvdata(pdev, sdev); 
    dev_info(sdev->dev, "NXP SimTemp probed: Sampling=%u ms, Threshold=%d mC. Device /dev/simtemp created.\n", 
             sdev->sampling_ms, sdev->threshold_mC);

    return 0;
}

static int nxp_simtemp_remove(struct platform_device *pdev)
{
    struct nxp_simtemp_dev *sdev = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "NXP SimTemp driver removing device /dev/simtemp.\n");
    
    // Unregister the misc device (Crucial cleanup step)
    misc_deregister(&sdev->misc_dev);

    // TODO: Stop hrtimer/workqueue here (to be added in Sprint 6)
    
    // devm_kzalloc handles memory cleanup, so no kfree needed here.
    return 0;
}

// --- 5. Platform Driver Registration ---

static const struct of_device_id nxp_simtemp_of_match[] = {
    { .compatible = "nxp,simtemp", }, 
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nxp_simtemp_of_match);

static struct platform_driver nxp_simtemp_driver = {
    .probe          = nxp_simtemp_probe,
    .remove         = nxp_simtemp_remove,
    .driver = {
        .name       = "nxp-simtemp", 
        .of_match_table = nxp_simtemp_of_match,
    },
};

// --- 6. Module Entry and Exit ---

static int __init nxp_simtemp_init(void)
{
    return platform_driver_register(&nxp_simtemp_driver);  
}

static void __exit nxp_simtemp_exit(void)
{
    platform_driver_unregister(&nxp_simtemp_driver);
}

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

// Standard Module Metadata
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ricardo Loya"); 
MODULE_DESCRIPTION("NXP Simulated Temperature Sensor Platform Driver.");