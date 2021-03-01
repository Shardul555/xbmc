include ../../Makefile.include
DEPS= ../../Makefile.include Makefile ID3v2Lookup.patch remove-boost.patch

LIBNAME=taglib
VERSION=1.11.1
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(LIBNAME)-$(VERSION).tar.gz

# ToDo: Bump Android NDK minimum to 21e LTS
# taglib 1.12 for android requires minimum NDK of 21e
# android NDK 20b cmath header has a c preprocessor error that makes it unable to 
# compile taglib 1.12.
ifneq ($(OS),android)
VERSION=1.12
BASE_URL=https://taglib.org/releases
endif

LIBDYLIB=$(PLATFORM)/build/$(LIBNAME)/libtag.a
all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); rm -rf build; mkdir -p build
ifeq ($(OS),android)
# patches are already in 1.12, so remove when android NDK bump happens
	cd $(PLATFORM); patch -p1 -i ../ID3v2Lookup.patch
	cd $(PLATFORM); patch -p1 -i ../remove-boost.patch
endif
	cd $(PLATFORM)/build; $(CMAKE) -DCMAKE_LIBRARY_PATH=$(TOOLCHAIN)/lib/$(HOST) -DBUILD_SHARED_LIBS=OFF ..

$(LIBDYLIB): $(PLATFORM)
	$(MAKE) -C $(PLATFORM)/build
	touch $@
.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM)/build install
	rm -f $(PREFIX)/lib/libtag_c.*
	touch $@

clean:
	$(MAKE) -C $(PLATFORM)/build clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
