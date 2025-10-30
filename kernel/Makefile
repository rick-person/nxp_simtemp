# File: kernel/Makefile

# ----------------------------------------------------
# 1. Module Name Definition
# ----------------------------------------------------
# Tells the kernel build system which objects to build.
obj-m := nxp_simtemp.o

# ----------------------------------------------------
# 2. Source Files for the Module
# ----------------------------------------------------
# Compile our C file and include our IOCTL header
nxp_simtemp-objs := nxp_simtemp.c

# ----------------------------------------------------
# 3. Build Targets
# ----------------------------------------------------
# Detect the external kernel source path (KDIR)
KDIR ?= $(KERNEL_SRC)
ifeq ($(KDIR),)
    # Fallback to the currently running kernel's headers if KDIR is not set
    KDIR := /lib/modules/$(shell uname -r)/build
endif

# Default action is to build the module using the kernel build system
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean target to remove compiled artifacts
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Phony targets to prevent conflicts with files named 'all' or 'clean'
.PHONY: all clean