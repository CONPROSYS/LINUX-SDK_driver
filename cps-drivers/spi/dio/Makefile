obj-m := cpsdio_spi.o

PWD       := $(shell pwd)

# CPS_SDK_MACRO_CHECK ( without CONPROSYS SDK )
ifneq "$(CPS_SDK_INSTALL_FULLDIR)" ""
TARGET_ROOTFS   := rootfs
endif

EXTRA_CFLAGS	:= -DDEBUG=0

ifeq "$(CPS_SDK_ROOTDIR)" ""
#KERNELDIR ?= /lib/modules/$(shell uname -r)/build　##target kernel source path ( self build )
KERNELDIR ?= /home/contec/workspace/sdk/CPS_SDK/kernel #target kernel source path ( static path )
#KERNELDIR ?= ~/CPS_SDK/kernel #target kernel source path ( static path )
CPS_DRIVER_REELASE_DIR:=../../release/mc341/
EXTRA_CFLAGS +=-I$(src)/../../../include/
else
KERNELDIR ?= $(CPS_SDK_ROOTDIR)/kernel
CPS_DRIVER_REELASE_DIR:=$(CPS_SDK_ROOTDIR)/driver/release/mc341/ 
EXTRA_CFLAGS +=-I$(src)/../include/
endif

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install 

release_copy:
	cp -p ./cpsdio_spi.ko $(CPS_DRIVER_REELASE_DIR)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions vtty Module.symvers modules.order 
