obj-m := cps-driver.o

#KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

KERNELDIR ?= $(CPS_SDK_ROOTDIR)/kernel #target kernel source path

EXTRA_CFLAGS	:= -DDEBUG=0
EXTRA_CFLAGS	+= -I$(CPS_SDK_ROOTDIR)/driver/cps-drivers/include

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(DRIVER_INCLUDES) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

release_copy:
	cp -p ./cps-driver.ko ../../release/
	
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions vtty Module.symvers modules.order 
