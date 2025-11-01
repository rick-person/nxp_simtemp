# NXP Simulated Temperature Sensor Driver: DESIGN.md

## 1. System Architecture and Interaction Model

The system utilizes a standard **Producer-Consumer model** that is fully mediated by the Linux kernel's character device and synchronization mechanisms.

The primary architectural goal was to ensure **concurrency safety** and **non-blocking asynchronous event delivery**.

### Block Diagram of Interaction


### Component Interaction Description

1.  **Producer (Kernel):** The **High-Resolution Timer (`hrtimer`)** periodically executes the `simtemp_timer_handler`. This function:
    * Simulates temperature drift.
    * Generates a binary `struct simtemp_sample`.
    * Writes the sample to the **Bounded Ring Buffer**.
    * Checks if the new sample crosses the configured threshold.
    * Wakes up the **Wait Queue (`read_queue`)** to signal user-space.

2.  **Consumer (User-space CLI):** The Python application opens `/dev/simtemp` and uses the **`poll()`** system call to efficiently sleep.
    * When woken by the kernel, the CLI performs a **blocking read** (`os.read()`) to consume the binary sample from the kernel's ring buffer.

3.  **Configuration Path:** User-space can configure the driver through two distinct paths:
    * **Sysfs:** Used for simple, inspectable, human-readable controls (`sampling_ms`, `threshold_mC`, `mode`).
    * **IOCTL:** Used for atomic, binary configuration calls, bypassing Sysfs string parsing overhead.

---

## 2. API and Design Choices

### 2.1 Concurrency and Locking Model

We chose **Spinlocks** (`spinlock_t lock`) for concurrency management.

* **Rationale:** Spinlocks are necessary because the primary contention point—access to the **ring buffer indices (`head`/`tail`)** and shared configuration variables—occurs within the **atomic context** of the **HRTimer handler**. Operations are extremely brief (just a few read/writes of integers and struct copies) and do not involve sleeping.
* **Implementation:** We use `spin_lock_irqsave()` and `spin_unlock_irqrestore()` within the timer handler to temporarily disable local interrupts, ensuring total exclusivity and preventing deadlocks between the timer's interrupt context and the user-space context (read, ioctl, sysfs).

### 2.2 User-Kernel API Trade-offs

| Interface | Rationale | Why Not Other? |
| :--- | :--- | :--- |
| **Data Transfer (`read`)** | **Binary Record:** Provides maximum efficiency and avoids string parsing overhead at the kernel level. | Writing temperature data via `sysfs` is too slow and inefficient for high-frequency samples. |
| **Event Signaling (`poll`)** | **Asynchronous and Efficient:** Allows the CLI to wait with zero CPU usage. We use `EPOLLIN` for new data and `EPOLLPRI` for the alert event. | Polling (busy-waiting) is highly inefficient and wastes CPU cycles. |
| **Control (`sysfs`)** | **Debuggability:** Easy human inspection (`cat`) and simple configuration (`echo`). Primary configuration path. | IOCTL is less intuitive for manual debugging. |

### 2.3 Device Tree Mapping

| DT Property | C Struct Field | Rationale / Default |
| :--- | :--- | :--- |
| **`compatible`** | `"nxp,simtemp"` | Required by the `of_match_table` for the driver to bind to the device node. |
| **`sampling-ms`** | `sdev->sampling_ms` (`u32`) | Configures the **HRTimer period** (e.g., 100 ms). **Default:** 100. |
| **`threshold-mC`** | `sdev->threshold_mC` (`s32`) | Configures the temperature value that triggers the **`POLLPRI` alert** and sets the flag in the sample record. **Default:** 45000 m°C. |

### 2.4 Scaling and Mitigation (Problem-Solving Write-Up)

**What breaks first at 10 kHz sampling (100 µs period)?**

1.  **Context Switching:** The kernel will be forced to switch contexts 10,000 times per second, rapidly degrading overall system performance (a common bottleneck in timer-based systems).
2.  **Ring Buffer Overflow:** If user-space cannot read samples fast enough, the fixed-size ring buffer will constantly overflow, resulting in dropped data.
3.  **Wait Queue Thrashing:** Waking up the wait queue 10,000 times per second creates massive overhead.

**Strategies to Mitigate:**

* **Averaging/Buffering:** Implement a buffer limit in the timer handler. If the buffer is nearly full, **skip generating samples** until the consumer catches up, rather than overwriting existing samples.
* **Interrupt Coalescing:** Switch from waking on every sample to only waking the consumer when the buffer reaches **50% capacity** or when the **threshold alert** occurs. This significantly reduces wake-ups and context switching overhead. (The current design implements per-sample wake-ups for simplicity.)
* **Dedicated Workqueue (Stretch):** Offload sample processing (simulation, threshold check) from the HRTimer's interrupt context to a dedicated workqueue to minimize latency in the interrupt handler.

---

**[End of DESIGN.md]**