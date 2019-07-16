obj-m := 8250_cpscom.o

PWD       := $(shell pwd)

# CPS_SDK_MACRO_CHECK ( without CONPROSYS SDK )
ifneq "$(CPS_SDK_INSTALL_FULLDIR)" ""
TARGET_ROOTFS   := rootfs
endif

EXTRA_CFLAGS	:= -DDEBUG=0
EXTRA_CFLAGS	+= -I$(CPS_SDK_ROOTDIR)/driver/cps-drivers/include

#set architecture
ifeq "${ARCH}" ""
 ifeq "${CPS_SDK_TARGET_NAME}" ""
  # without CONPROSYS Linux SDK
  ARCH=arm
  CROSS_COMPILE=arm-linux-gnueabi-
 else
  # CONPROSYS Linux SDK
  ARCH=$(shell arch)
 endif
endif

ifeq "${CROSS_COMPILE}" "arm-linux-gnueabihf-"
	HF_OPT=hf
endif


ifeq "$(CPS_SDK_ROOTDIR)" ""
#KERNELDIR ?= /lib/modules/$(shell uname -r)/build　##target kernel source path ( self build )
KERNELDIR ?= /home/contec/workspace/sdk/CPS_SDK/kernel #target kernel source path ( static path )
CPS_DRIVER_REELASE_DIR:=../release/mcs341/$(ARCH)$(HF_OPT)/
else
KERNELDIR ?= $(CPS_SDK_ROOTDIR)/kernel
CPS_DRIVER_REELASE_DIR:=$(CPS_SDK_ROOTDIR)/driver/release/mcs341/$(ARCH)$(HF_OPT)/
EXTRA_CFLAGS	+= -DCONFIG_CONPROSYS_SDK
endif

#cps-system extra symbol path
EXTRA_SYMBOL	:= $(PWD)/../system/Module.symvers

all:
	$(MAKE) -C $(KERNELDIR) KBUILD_EXTRA_SYMBOLS=$(EXTRA_SYMBOL) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

release_copy:
	cp -p ./8250_cpscom.ko $(CPS_DRIVER_REELASE_DIR)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions modules.order Module.symvers
