obj-m := gpmc_testd.o

#self build 
#GCC=gcc
#cross build
GCC=arm-linux-gnueabi-gcc

EXTRA_CFLAGS	:= -DDEBUG=0

all:
	$(GCC) -o gpmc_testd gpmc_testd.c

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions vtty Module.symvers modules.order 
