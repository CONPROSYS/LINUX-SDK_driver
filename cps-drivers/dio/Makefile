obj-m := cpsdio.o

#KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

#cross build 
KERNELDIR ?= $(CPS_SDK_ROOTDIR)/kernel #target kernel source path

EXTRA_CFLAGS	:= -DDEBUG=0
EXTRA_CFLAGS	+= -I$(CPS_SDK_ROOTDIR)/driver/cps-drivers/include

#cps-system extra symbol path
EXTRA_SYMBOL	:= $(PWD)/../system/Module.symvers

all:
	$(MAKE) -C $(KERNELDIR) KBUILD_EXTRA_SYMBOLS=$(EXTRA_SYMBOL) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

release_copy:
	cp -p ./cpsdio.ko ../../release/

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions vtty Module.symvers modules.order 
