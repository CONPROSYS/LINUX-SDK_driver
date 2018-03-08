obj-m := cpsaio.o

PWD       := $(shell pwd)

#cross build 

# CPS_SDK_MACRO_CHECK ( without CONPROSYS SDK )
ifneq "$(CPS_SDK_INSTALL_FULLDIR)" ""
TARGET_ROOTFS   := rootfs
endif

EXTRA_CFLAGS	:= -DDEBUG=0
EXTRA_CFLAGS	+= -I$(CPS_SDK_ROOTDIR)/driver/cps-drivers/include

ifeq "$(CPS_SDK_ROOTDIR)" ""
#KERNELDIR ?= /lib/modules/$(shell uname -r)/build　##target kernel source path ( self build )
KERNELDIR ?= /home/contec/workspace/sdk/CPS_SDK/kernel #target kernel source path ( static path )
#KERNELDIR ?= ~/CPS_SDK/kernel #target kernel source path ( static path )
CPS_DRIVER_REELASE_DIR:=../release/mcs341/
else
KERNELDIR ?= $(CPS_SDK_ROOTDIR)/kernel
CPS_DRIVER_REELASE_DIR:=$(CPS_SDK_ROOTDIR)/driver/release/mcs341/
EXTRA_CFLAGS	+= -DCONFIG_CONPROSYS_SDK
endif

# double and float calc option (soft-float version)
KBUILD_CFLAGS += -mfloat-abi=softfp
KBUILD_AFLAGS += -mfloat-abi=softfp

#cps-system extra symbol path
EXTRA_SYMBOL	:= $(PWD)/../system/Module.symvers

all:
	$(MAKE) -C $(KERNELDIR) KBUILD_EXTRA_SYMBOLS=$(EXTRA_SYMBOL) M=$(PWD) modules 

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

release_copy:
	cp -p ./cpsaio.ko $(CPS_DRIVER_REELASE_DIR)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions vtty Module.symvers modules.order 
