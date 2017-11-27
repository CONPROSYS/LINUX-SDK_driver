subdirs = system com dio aio ssi
MAKE=make --no-print-directory -e

all:	
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE)) ;\
	done

modules_install:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) modules_install ) ;\
	done

release_copy:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) release_copy ) ;\
	done

sdk_install:
	cp -p include/*  ${CPS_SDK_INSTALL_FULLDIR}/${CPS_SDK_ROOTFS}/usr/local/include

clean:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) clean ) ;\
	done
