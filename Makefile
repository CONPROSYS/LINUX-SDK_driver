subdirsall = cps-drivers cps-iolib
ifeq ($(CPS_SDK_PRODUCT_TYPE),CPS-MCS341-DSX)
  subdirs = cps-drivers cps-iolib
endif
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
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) sdk_install ) ;\
	done

clean:
	@for subdir in $(subdirsall) ; do \
	(cd $$subdir && $(MAKE) clean ) ;\
	done
