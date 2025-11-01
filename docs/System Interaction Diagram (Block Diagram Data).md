### 1.1 System Interaction Diagram (Block Diagram Data)

The system operates as a **Producer-Consumer model** mediated by the Linux kernel's character device framework.

#### Primary Components and Data Flow

| Component | Role | Synchronization Mechanism |
| :--- | :--- | :--- |
| **HRTimer (Producer)** | Generates simulated samples periodically. | Writes to shared memory (`Ring Buffer`) under a **Spinlock**. |
| **Ring Buffer** | Bounded memory queue for temperature data. | Protected by **Spinlock** (`sdev->lock`). |
| **Wait Queue** | Event signaling mechanism. | Wakes up user-space when data is written or an alert occurs. |
| **User CLI (Consumer)** | Reads samples and configures the driver. | Blocks using **`poll()`/`epoll`** until woken by the **Wait Queue**. |
| **Sysfs/IOCTL** | Configuration interface. | Interacts with shared configuration data (`threshold_mC`) under a **Spinlock**. |

#### Interaction Flow Description (for Graph Generation)

The flow visually represents the path of data (high frequency) and control/events (low frequency):

1.  **Production Cycle:** The **HRTimer** runs periodically in kernel space, generates a sample, and writes it to the **Ring Buffer** while holding the **Spinlock**.
2.  **Event Signaling:** After writing, the timer handler checks the threshold and calls **`wake_up_interruptible()`** on the **Wait Queue**, signaling to the consuming process.
3.  **Consumption:** The **User CLI** is blocked in the **`poll()`** system call, registered to the Wait Queue. The wake-up unblocks the CLI, which executes a **blocking `read()`** to retrieve the binary data from the Ring Buffer.
4.  **Configuration:** The **User CLI** modifies driver behavior by writing values to **Sysfs** attributes (e.g., `sampling_ms`) or executing an **IOCTL** command. These paths also acquire the **Spinlock** to safely update the shared configuration variables.



---