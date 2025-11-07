# File: kernel/Makefile

# ----------------------------------------------------
# 1. Module Name Definition
# ----------------------------------------------------
obj-m := nxp_simtemp.o

# ----------------------------------------------------
# 2. Source Files for the Module (Corrected CFLAGS)
# ----------------------------------------------------

# CRITICAL FIX: Aggressively disable ALL sanitizers and problematic warnings.
KBUILD_CFLAGS += -Wno-unused-variable
KBUILD_CFLAGS += -Wno-declaration-after-statement
KBUILD_CFLAGS += -Wno-pointer-sign
KBUILD_CFLAGS += -Wno-address-of-packed-member
KBUILD_CFLAGS += -fno-stack-protector
# Corrected syntax for AddressSanitizer
KBUILD_CFLAGS += -fno-sanitize=address 
# Explicitly disable other sanitizers seen in verbose output
KBUILD_CFLAGS += -fno-sanitize=bounds-strict
KBUILD_CFLAGS += -fno-sanitize=shift
KBUILD_CFLAGS += -fno-sanitize=bool
KBUILD_CFLAGS += -fno-sanitize=enum
# KBUILD_CFLAGS += -fno-ubsan

# ----------------------------------------------------------------------------------

# ----------------------------------------------------------------------------------

# ----------------------------------------------------
# 3. Build Targets
# ----------------------------------------------------
KDIR ?= $(KERNEL_SRC) 

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean