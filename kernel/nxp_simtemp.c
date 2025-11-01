#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h> 
#include <linux/slab.h> 
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/miscdevice.h> 
#include <linux/io.h>       
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/uaccess.h> // For copy_from_user/copy_to_user
#include <linux/ioctl.h>
#include <linux/device.h> // For device_attribute and sysfs
#include <linux/sysfs.h>  // For sysfs_create_groups
#include <linux/jiffies.h> // For prandom_u32
#include "nxp_simtemp.h"    
#include "nxp_simtemp_ioctl.h" 

// --- Redundancy Cleanup and Global State ---

// define the sysfs file permissions (RW)
#define SDEV_ATTR_RW(_name) DEVICE_ATTR(_name, S_IWUSR | S_IRUGO, _name##_show, _name##_store)
// define the sysfs file permissions (RO)
#define SDEV_ATTR_RO(_name) DEVICE_ATTR(_name, S_IRUGO, _name##_show, NULL)

// Global state for simulating temperature changes (for the producer)
static s32 global_sim_temp = 42000;
static const struct attribute_group *simtemp_attr_groups[]; // Prototype for sysfs integration

struct nxp_simtemp_dev {
    struct device *dev;
    struct miscdevice misc_dev;
    struct simtemp_ring_buffer ring_buf;
    
    // Configuration
    u32 sampling_ms;
    s32 threshold_mC;
    int mode;
    
    // Status and Timer
    __u32 status_flags; // Used by poll and ioctl
    spinlock_t lock;
    wait_queue_head_t read_queue;

    struct hrtimer timer;
    ktime_t period;
    s32 current_temp_mC;
};

// --- Function Prototypes ---
static long simtemp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static int simtemp_open(struct inode *inode, struct file *file);
static int simtemp_release(struct inode *inode, struct file *file);
static __poll_t simtemp_poll(struct file *file, poll_table *wait);

// --- 2. Character Device File Operations ---

// FIX 1: Corrected simtemp_open logic to safely retrieve the private data structure (sdev)
static int simtemp_open(struct inode *inode, struct file *file) 
{ 
    struct nxp_simtemp_dev *sdev;

    // CRITICAL FIX: The miscdevice is embedded in sdev, and its internals point to sdev.
    // We use the inode's cdev field to get the containing miscdevice, and then the sdev container.
    sdev = container_of(inode->i_cdev, struct nxp_simtemp_dev, misc_dev.this_device);
    
    // Store sdev in file->private_data for use by read/ioctl/poll
    file->private_data = sdev; 
    
    dev_dbg(sdev->dev, "Device opened.\n");

    return 0;
}

static int simtemp_release(struct inode *inode, struct file *file) 
{ 
    // Usually decrements an open counter
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

    // 2. Wait for Data (Correct Blocking/Non-Blocking Flow)
    if (sdev->ring_buf.head == sdev->ring_buf.tail) {
        
        if (file->f_flags & O_NONBLOCK) {
            return -EAGAIN; 
        }

        // Blocking read only happens here, in the 'blocking' path
        ret = wait_event_interruptible(sdev->read_queue, 
                                       sdev->ring_buf.head != sdev->ring_buf.tail);
        
        if (ret == -ERESTARTSYS)
            return ret; // Interrupted by signal
    }

    // 3. Consume Sample 
    spin_lock_irqsave(&sdev->lock, flags);
    
    // Final check for data availability after the wait
    if (sdev->ring_buf.head == sdev->ring_buf.tail) {
        spin_unlock_irqrestore(&sdev->lock, flags);
        return 0; // No data available 
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

    spin_lock(&sdev->lock);

    switch (cmd) {
    case SIMTEMP_SET_THRESHOLD:
        if (copy_from_user(&value, user_arg, sizeof(value))) {
            ret = -EFAULT;
            goto unlock;
        }
        sdev->threshold_mC = value;
        dev_dbg(sdev->dev, "IOCTL: Set threshold to %d mC\n", sdev->threshold_mC);
        break;

    case SIMTEMP_SET_SAMPLING:
        if (copy_from_user(&value, user_arg, sizeof(value))) {
            ret = -EFAULT;
            goto unlock;
        }
        if (value <= 0) { 
            ret = -EINVAL;
            goto unlock;
        }
        sdev->sampling_ms = value;
        // NOTE: Timer must be re-armed after this change.
        dev_dbg(sdev->dev, "IOCTL: Set sampling to %u ms\n", sdev->sampling_ms);
        break;

    case SIMTEMP_GET_STATUS:
        if (copy_to_user(user_arg, &sdev->status_flags, sizeof(sdev->status_flags))) {
            ret = -EFAULT;
            goto unlock;
        }
        break;

    default:
        ret = -ENOTTY;
        break;
    }

unlock: 
    spin_unlock(&sdev->lock);
    return ret;
}


static __poll_t simtemp_poll(struct file *file, poll_table *wait)
{
    struct nxp_simtemp_dev *sdev = file->private_data;
    __poll_t mask = 0;
    unsigned long flags;

    poll_wait(file, &sdev->read_queue, wait);

    spin_lock_irqsave(&sdev->lock, flags);

    if (sdev->ring_buf.head != sdev->ring_buf.tail) {
        mask |= EPOLLIN | EPOLLRDNORM;
    }

    if (sdev->status_flags & SIMTEMP_FLAG_THRESHOLD_CROSSED) {
        mask |= EPOLLPRI; 
    }

    spin_unlock_irqrestore(&sdev->lock, flags);

    return mask;
}


// Helper to update timer after writing a new period
static void simtemp_update_timer(struct nxp_simtemp_dev *sdev)
{
    if (hrtimer_active(&sdev->timer))
        hrtimer_cancel(&sdev->timer);

    sdev->period = ms_to_ktime(sdev->sampling_ms);
    hrtimer_start(&sdev->timer, sdev->period, HRTIMER_MODE_REL);
}

// ----------------------------------------------------
// a) sampling_ms (RW)
// ----------------------------------------------------
static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct nxp_simtemp_dev *sdev = dev_get_drvdata(dev);
    return sprintf(buf, "%u\n", sdev->sampling_ms);
}

static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct nxp_simtemp_dev *sdev = dev_get_drvdata(dev);
    unsigned long val;

    if (kstrtoul(buf, 10, &val) || val == 0)
        return -EINVAL;

    spin_lock(&sdev->lock);
    sdev->sampling_ms = (u32)val;
    spin_unlock(&sdev->lock);

    simtemp_update_timer(sdev);
    return count;
}
SDEV_ATTR_RW(sampling_ms);


// ----------------------------------------------------
// b) threshold_mC (RW)
// ----------------------------------------------------
static ssize_t threshold_mC_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct nxp_simtemp_dev *sdev = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", sdev->threshold_mC);
}

static ssize_t threshold_mC_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct nxp_simtemp_dev *sdev = dev_get_drvdata(dev);
    long val;

    if (kstrtol(buf, 10, &val))
        return -EINVAL;
    
    spin_lock(&sdev->lock);
    sdev->threshold_mC = (s32)val;
    spin_unlock(&sdev->lock);
    
    return count;
}
SDEV_ATTR_RW(threshold_mC);


// ----------------------------------------------------
// c) stats (RO)
// ----------------------------------------------------
static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct nxp_simtemp_dev *sdev = dev_get_drvdata(dev);
    // NOTE: For MVP, we only expose the current status flags
    return sprintf(buf, "Status Flags: 0x%X\n", sdev->status_flags);
}
SDEV_ATTR_RO(stats);


// ----------------------------------------------------
// d) mode (RW) - Simple stub
// ----------------------------------------------------
static ssize_t mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct nxp_simtemp_dev *sdev = dev_get_drvdata(dev);
    const char *mode_str;
    switch (sdev->mode) {
        case 0: mode_str = "normal"; break;
        case 1: mode_str = "noisy"; break;
        default: mode_str = "unknown"; break;
    }
    return sprintf(buf, "%s\n", mode_str);
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct nxp_simtemp_dev *sdev = dev_get_drvdata(dev);

    // Simple check for "normal" or "noisy"
    if (sysfs_streq(buf, "normal")) {
        sdev->mode = 0;
    } else if (sysfs_streq(buf, "noisy")) {
        sdev->mode = 1;
    } else {
        return -EINVAL;
    }
    
    return count;
}
SDEV_ATTR_RW(mode);


// ----------------------------------------------------
// e) Attribute Group and Setup
// ----------------------------------------------------
static struct attribute *simtemp_attrs[] = {
    &dev_attr_sampling_ms.attr,
    &dev_attr_threshold_mC.attr,
    &dev_attr_stats.attr,
    &dev_attr_mode.attr,
    NULL,
};

static struct attribute_group simtemp_attr_group = {
    .attrs = simtemp_attrs,
};

static const struct attribute_group *simtemp_attr_groups[] = {
    &simtemp_attr_group,
    NULL,
};

enum hrtimer_restart simtemp_timer_handler(struct hrtimer *timer)
{
    struct nxp_simtemp_dev *sdev = container_of(timer, struct nxp_simtemp_dev, timer);
    unsigned long flags;
    struct simtemp_sample sample;
    unsigned int next_head;

    // 1. Simulate new temperature and check threshold
    sdev->current_temp_mC = global_sim_temp + (prandom_u32() % 1000 - 500);

    sample.temp_mC = sdev->current_temp_mC;
    sample.flags = SIMTEMP_FLAG_NEW_SAMPLE;
    
    if (sdev->current_temp_mC > sdev->threshold_mC) {
        sample.flags |= SIMTEMP_FLAG_THRESHOLD_CROSSED;
        sdev->status_flags |= SIMTEMP_FLAG_THRESHOLD_CROSSED;
    } else {
        sdev->status_flags &= ~SIMTEMP_FLAG_THRESHOLD_CROSSED;
    }

    sample.timestamp_ns = ktime_get_real_ns();

    // 2. Write to Ring Buffer (Critical Section)
    spin_lock_irqsave(&sdev->lock, flags);

    next_head = (sdev->ring_buf.head + 1) % SIMTEMP_MAX_SAMPLES;
    
    // Check for overflow (if head catches tail, we drop the oldest sample)
    if (next_head == sdev->ring_buf.tail) {
        sdev->ring_buf.tail = (sdev->ring_buf.tail + 1) % SIMTEMP_MAX_SAMPLES;
    }

    // Write sample and advance head
    sdev->ring_buf.buf[sdev->ring_buf.head] = sample;
    sdev->ring_buf.head = next_head;
    
    spin_unlock_irqrestore(&sdev->lock, flags);

    // 3. Wake Up Consumers
    if (sample.flags & SIMTEMP_FLAG_NEW_SAMPLE) {
        wake_up_interruptible(&sdev->read_queue); 
    }

    // 4. Re-arm the timer for the next period
    hrtimer_forward_now(timer, sdev->period);

    return HRTIMER_RESTART;
}

static const struct file_operations simtemp_fops = {
    .owner              = THIS_MODULE,
    .open               = simtemp_open,
    .release            = simtemp_release,
    .read               = simtemp_read,     
    .unlocked_ioctl     = simtemp_ioctl,    
    .poll               = simtemp_poll,
};

// --- 3. Misc Device Registration Structure ---
static struct miscdevice simtemp_misc_dev = {
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "simtemp",
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
    sdev->current_temp_mC = 25000;
    sdev->status_flags = 0; // Initialize status flags

    // 4.2 Read DT Properties
    if (np) {
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

    // 4.3 HRTIMER SETUP
    sdev->period = ms_to_ktime(sdev->sampling_ms); 
    hrtimer_init(&sdev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    sdev->timer.function = simtemp_timer_handler;

    // 4.4 Register Misc Device 
    sdev->misc_dev = simtemp_misc_dev;
    sdev->misc_dev.name = "simtemp";   
    sdev->misc_dev.fops = &simtemp_fops; 
    sdev->misc_dev.minor = MISC_DYNAMIC_MINOR;

    ret = misc_register(&sdev->misc_dev);
    
    if (ret) {
        dev_err(sdev->dev, "Failed to register misc device: %d\n", ret);
        return ret;
    }

    // 4.5 Sysfs Creation
    ret = sysfs_create_groups(&pdev->dev.kobj, simtemp_attr_groups);
    if (ret) {
        dev_err(sdev->dev, "Failed to create sysfs groups: %d\n", ret);
        goto err_dereg_misc;
    }

    // 4.6 Finalize & Start timer
    platform_set_drvdata(pdev, sdev); 

    hrtimer_start(&sdev->timer, sdev->period, HRTIMER_MODE_REL);

    dev_info(sdev->dev, "NXP SimTemp probed: Sampling=%u ms, Threshold=%d mC. Device /dev/simtemp created.\n", 
             sdev->sampling_ms, sdev->threshold_mC);

    return 0;

err_dereg_misc:
    misc_deregister(&sdev->misc_dev);
    return ret;
}

static int nxp_simtemp_remove(struct platform_device *pdev)
{
    struct nxp_simtemp_dev *sdev = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "NXP SimTemp driver removing device /dev/simtemp.\n");
    
    // 1. Stop the Timer (CRITICAL CLEANUP)
    hrtimer_cancel(&sdev->timer);
    
    // 2. Remove Sysfs nodes
    sysfs_remove_groups(&pdev->dev.kobj, simtemp_attr_groups);

    // 3. Unregister the misc device
    misc_deregister(&sdev->misc_dev);

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