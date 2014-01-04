ifeq ($(XEN_TARGET_ARCH),arm32)
ARCH_CFLAGS  := -march=armv7-a -marm -fms-extensions -D__arm__ #-DCPU_EXCLUSIVE_LDST
EXTRA_INC += $(TARGET_ARCH_FAM)/$(XEN_TARGET_ARCH)
EXTRA_SRC += arch/$(EXTRA_INC)
endif

