subdirs = CalibCommand Calibration measure queryDevice
MAKE=make --no-print-directory

all:	
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE)) ;\
	done

clean:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) clean ) ;\
	done
