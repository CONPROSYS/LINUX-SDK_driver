obj-m := cps-iolib.o

PWD       := $(shell pwd)

#cross build 
CROSS_COMPILE:=arm-linux-gnueabi-
ARCH=arm

# CPS_SDK_MACRO_CHECK ( without CONPROSYS SDK )
ifneq "$(CPS_SDK_INSTALL_FULLDIR)" ""
TARGET_ROOTFS   := rootfs
endif

EXTRA_CFLAGS	:= -DDEBUG=0

ifeq "$(CPS_SDK_ROOTDIR)" ""
#KERNELDIR ?= /lib/modules/$(shell uname -r)/build　##target kernel source path ( self build )
KERNELDIR ?= /home/contec/workspace/sdk/CPS_SDK/kernel #target kernel source path ( static path )
#KERNELDIR ?= ~/CPS_SDK/kernel #target kernel source path ( static path )
#CPS_IOLIB_REELASE_DIR:=../release/mcs341/
else
KERNELDIR ?= $(CPS_SDK_ROOTDIR)/kernel
#CPS_IOLIB_REELASE_DIR:=../../release/
#EXTRA_CFLAGS	+= -DCONFIG_CONPROSYS_SDK
endif
all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions vtty Module.symvers modules.order 
