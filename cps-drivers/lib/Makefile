subdirs = dio aio ssi cnt
MAKE=make --no-print-directory

all:	
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE)) ;\
	done

install:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) install ) ;\
	done

install-cross:
	@for subdir in $(subdirs) ; do \
	( cd $$subdir && $(MAKE) install-cross ) ;\
	done

release_copy:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) release_copy ) ;\
	done

clean:
	@for subdir in $(subdirs) ; do \
	(cd $$subdir && $(MAKE) clean ) ;\
	done
