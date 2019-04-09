subdirs = system aio com dio ssi cnt io spi/dio
MAKE=make --no-print-directory

all:	
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE)) || exit 1;\
	done

modules_install:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) modules_install ) || exit 1;\
	done

release_copy:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) release_copy ) || exit 1;\
	done

sdk_install:
	cp -p include/*  ${CPS_SDK_INSTALL_FULLDIR}/${CPS_SDK_ROOTFS}/usr/local/include

clean:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) clean ) || exit 1;\
	done
