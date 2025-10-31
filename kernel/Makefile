# File: kernel/Makefile (Fixed for Native Host Compilation)

# ----------------------------------------------------
# 1. Module Name Definition
# ----------------------------------------------------
obj-m := nxp_simtemp.o

# ----------------------------------------------------
# 2. Source Files for the Module
# ----------------------------------------------------
nxp_simtemp-objs := nxp_simtemp.c

# ----------------------------------------------------
# 3. Build Targets
# ----------------------------------------------------
# KDIR is set by the build.sh script.
KDIR ?= $(KERNEL_SRC) 

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean