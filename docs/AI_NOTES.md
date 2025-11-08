# AI Usage and Validation Notes

## 1. Summary of AI Use

AI assistance was utilized as a technical collaborator (Co-Pilot model, Gemini 2.5) to accelerate development, validate architectural choices, and critically, to debug complex environmental compilation issues specific to the host kernel's header files.

## 2. Specific Areas of Assistance

**The full text of the prompts used during the critical debugging process is contained within the history of this conversation.** The following table summarizes the purpose of those prompts:

| Area | Purpose | Outcome/Validation |
| :--- | :--- | :--- |
| **Initial Implementation** | Generating boilerplate for the `platform_driver` structure, `file_operations` table, and setting up Sysfs attributes. | Code structure was correct, requiring manual refinement of logical issues. |
| **API/Design Write-up** | Structuring the content for `DESIGN.md`, defining the rationale for **spinlocks**, and the use of **`poll`** vs. blocking reads. | Content was used as a robust template, then refined with project-specific details. |
| **CLI Implementation** | Translating the C `struct simtemp_sample` into the Python `struct.unpack` format and setting up the initial `poll` event handling loop. | Provided reliable binary parsing logic and standard Python I/O patterns. |

## 3. CRITICAL BUILD & RUNTIME DEBUGGING

The AI was instrumental in diagnosing and finding workarounds for failures stemming from corrupted/restrictive Linux kernel headers and environment security.

| Failure Point | Root Cause | Solution/Workaround |
| :--- | :--- | :--- |
| **Header Corruption** | `MODULE_LICENSE` macro failed compilation due to a broken internal link to `KBUILD_MODFILE`. | **Final Solution:** Manual injection of the license string into the object file using C source, bypassing the broken macro entirely. |
| **Linker Dependency** | `modpost` failed, reporting `__SCK__might_resched undefined!` after license fix. | **Final Solution:** Required removal of the blocking dependency (`wait_event_interruptible`) as the kernel failed to export the necessary scheduler symbol. |
| **Runtime Security** | Final `insmod` failed with **"Key was rejected by service."** (Secure Boot/Module Signature Verification). | **Final Analysis:** Determined to be an **unresolvable environmental security restriction** preventing the module from loading. This environmental roadblock is documented in the `README.md`. |

The final solution required combining multiple non-standard workarounds (manual symbol injection, aggressive CFLAGS removal) that were only achievable through iterative debugging against the live error log, demonstrating a necessary partnership between AI suggestion and human verification.