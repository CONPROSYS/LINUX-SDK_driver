obj-m := cps-iolib.o

#self-build
#CC=gcc
#KERNELDIR ?= /lib/modules/$(shell uname -r)/build 
PWD       := $(shell pwd)

EXTRA_CFLAGS	:= -DDEBUG=0

KERNELDIR ?= $(CPS_SDK_ROOTDIR)/kernel #target kernel source path

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

release_copy:
	cp -p ./cps-iolib.ko ../release/mcs341/

sdk_install:
	cp -p ./cps-iolib.h ${CPS_SDK_INSTALL_FULLDIR}/${CPS_SDK_ROOTFS}/usr/local/include

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions vtty Module.symvers modules.order 
