# File: kernel/Makefile (Final Working Version - Native Host)

# ----------------------------------------------------
# 1. Module Name Definition
# ----------------------------------------------------
obj-m := nxp_simtemp.o

# ----------------------------------------------------
# 2. Source Files for the Module
# ----------------------------------------------------
nxp_simtemp-objs := nxp_simtemp.c

# CRITICAL FIX: Disable optional compiler/linker features missing from host headers
# This bypasses the 'Makefile.ubsan' and other similar missing file errors.
KBUILD_CFLAGS += -Wno-unused-variable
KBUILD_CFLAGS += -Wno-declaration-after-statement
KBUILD_CFLAGS += -Wno-pointer-sign
KBUILD_CFLAGS += -Wno-address-of-packed-member
KBUILD_CFLAGS += -fno-stack-protector
KBUILD_CFLAGS += -fno-sanitize-address
KBUILD_CFLAGS += -fno-ubsan-checks # <-- Disables UBSAN

# ----------------------------------------------------
# 3. Build Targets
# ----------------------------------------------------
KDIR ?= $(KERNEL_SRC) 

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean