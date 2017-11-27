obj-m := cps-iolib.o

#self-build
#CC=gcc
#KERNELDIR ?= /lib/modules/$(shell uname -r)/build 
PWD       := $(shell pwd)

EXTRA_CFLAGS	:= -DDEBUG=0

#cross build 
CROSS_COMPILE=arm-linux-gnueabi-
ARCH=arm
KERNELDIR ?= $(CPS_SDK_ROOTDIR)/kernel #target kernel source path

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) modules_install

release_copy:
	cp -p ./cps-iolib.ko ../../release/

sdk_install:
	cp -p ./cps-iolib.h ${CPS_SDK_INSTALL_FULLDIR}/${CPS_SDK_ROOTFS}/usr/local/include

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions vtty Module.symvers modules.order 
