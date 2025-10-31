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
#include <linux/miscdevice.h> 
#include <linux/io.h>       
#include "nxp_simtemp.h"    
#include "nxp_simtemp_ioctl.h" // (Assuming this also exists)

// File: kernel/nxp_simtemp.c

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

// Function Prototypes for file_operations
static long simtemp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static int simtemp_open(struct inode *inode, struct file *file);

// --- 2. Character Device File Operations (Dummy for compilation) ---

static int simtemp_open(struct inode *inode, struct file *file) 
{ 
    // Retrieve the pointer to the private device structure from the miscdevice container
    struct nxp_simtemp_dev *sdev;

    sdev = container_of(file->f_op, struct nxp_simtemp_dev, misc_dev.fops);
    
    // Store sdev in file->private_data for use by read/ioctl/poll
    file->private_data = sdev; 
    
    dev_dbg(sdev->dev, "Device opened.\n");

    return 0;
}
static ssize_t simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{ 
    struct nxp_simtemp_dev *sdev = file->private_data;
    struct simtemp_sample sample;
    unsigned long flags;
    size_t sample_size = sizeof(struct simtemp_sample);
    int ret = 0;

    // 1. Basic Validation
    if (count < sample_size)
        return -EINVAL; // Must read at least one full binary record

    // 2. Wait for Data (Handle Blocking vs Non-Blocking)
    // The wait queue is woken up by the producer (timer) when new data is written.
    if (sdev->ring_buf.head == sdev->ring_buf.tail) {

        // If non-blocking flag (O_NONBLOCK) is set and there's no data after initial check:
        if (file->f_flags & O_NONBLOCK) {
            return -EAGAIN; 

        // Blocking read: wait until the head moves (data is available)
        ret = wait_event_interruptible(sdev->read_queue, 
                                       sdev->ring_buf.head != sdev->ring_buf.tail);
        
        if (ret == -ERESTARTSYS)
            return ret; // Wait was interrupted by a signal (e.g., Ctrl+C)
        }
    }

    // 3. Consume Sample 
    // Use spinlock to protect the shared ring buffer indices (head and tail).
    spin_lock_irqsave(&sdev->lock, flags);
    
    // Final check for data availability after the wait
    if (sdev->ring_buf.head == sdev->ring_buf.tail) {
        spin_unlock_irqrestore(&sdev->lock, flags);
        return 0; // No data available (shouldn't happen after a successful wait)
    }

    // Read the sample at the current tail index
    sample = sdev->ring_buf.buf[sdev->ring_buf.tail];
    
    // Advance the tail pointer (Ring buffer logic: wrap around)
    sdev->ring_buf.tail = (sdev->ring_buf.tail + 1) % SIMTEMP_MAX_SAMPLES;
    
    spin_unlock_irqrestore(&sdev->lock, flags);

    // 4. Copy Data to User Space
    if (copy_to_user(buf, &sample, sample_size)) {
        dev_err(sdev->dev, "Failed to copy sample data to user space.\n");
        return -EFAULT;
    }

    return sample_size; // Return the number of bytes successfully read
}

static long simtemp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        // Retrieve the private device data (sdev)
    struct nxp_simtemp_dev *sdev = file->private_data;
    void __user *user_arg = (void __user *)arg;
    __s32 value;
    int ret = 0;

    // 1. Validate the command magic number
    if (_IOC_TYPE(cmd) != NXP_SIMTEMP_MAGIC)
        return -ENOTTY;

    // 2. Validate user space pointer for write/read access
    if (_IOC_DIR(cmd) & (_IOC_WRITE | _IOC_READ)) {
        if (!access_ok(user_arg, _IOC_SIZE(cmd)))
            return -EFAULT;
    }

    // Acquire lock before modifying or reading shared state
    spin_lock(&sdev->lock);

    switch (cmd) {
    case SIMTEMP_SET_THRESHOLD:
        // Set Threshold: Read s32 value from user space
        if (copy_from_user(&value, user_arg, sizeof(value))) {
            ret = -EFAULT;
            goto unlock;
        }
        sdev->threshold_mC = value;
        // NOTE: Waking up the poll queue here is good practice if the threshold
        // change might immediately trigger an alert state.
        dev_dbg(sdev->dev, "IOCTL: Set threshold to %d mC\n", sdev->threshold_mC);
        break;

    case SIMTEMP_SET_SAMPLING:
        // Set Sampling Period: Read u32 value from user space
        if (copy_from_user(&value, user_arg, sizeof(value))) {
            ret = -EFAULT;
            goto unlock;
        }
        // Validation: Sampling period must be non-zero
        if (value <= 0) { 
            ret = -EINVAL;
            goto unlock;
        }
        sdev->sampling_ms = value;
        // NOTE: Timer must be re-armed after this change.
        dev_dbg(sdev->dev, "IOCTL: Set sampling to %u ms\n", sdev->sampling_ms);
        break;

    case SIMTEMP_GET_STATUS:
        // Get Status: Write s32 status flags to user space
        if (copy_to_user(user_arg, &sdev->status_flags, sizeof(sdev->status_flags))) {
            ret = -EFAULT;
            goto unlock;
        }
        break;

    default:
        ret = -ENOTTY; // Command not supported
        break;
    }

unlock: // <--- Single Exit Point for Lock Release
    spin_unlock(&sdev->lock);
    return ret;
}



static const struct file_operations simtemp_fops = {
    .owner              = THIS_MODULE,
    .open               = simtemp_open,
    .release            = simtemp_release,
    .read               = simtemp_read,     
    .unlocked_ioctl     = simtemp_ioctl,    
    // .poll will be added later
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