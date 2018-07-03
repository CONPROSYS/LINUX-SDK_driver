subdirsall = cps-drivers cps-iolib
ifeq ($(CPS_SDK_PRODUCT_TYPE),CPS-MCS341G-DSX)
  subdirs = cps-drivers cps-iolib
endif
ifeq ($(CPS_SDK_PRODUCT_TYPE),CPS-MCS341-DSX)
  subdirs = cps-drivers cps-iolib
endif
ifeq ($(CPS_SDK_PRODUCT_TYPE),CPS-MCS341Q-DSX)
  subdirs = cps-drivers cps-iolib
endif


MAKE=make --no-print-directory 

all:	
	echo $(subdirs)
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE)) || exit 1 ;\
	done

modules_install:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) modules_install )|| exit 1;\
	done

release_copy:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) release_copy ) || exit 1 ;\
	done

sdk_install:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) sdk_install ) || exit 1 ;\
	done

clean:
	@for subdir in $(subdirsall) ; do \
	(cd $$subdir && $(MAKE) clean ) || exit 1 ;\
	done
