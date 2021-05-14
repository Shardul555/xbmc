include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

# lib name, version
LIBNAME=yaml-cpp
VERSION=0.6.2-0f9a586-p1
SOURCE=v$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

# TODO
BASE_URL=https://github.com/hunter-packages/yaml-cpp/archive

# configuration settings
#export CXXFLAGS+=-DSQLITE_ENABLE_COLUMN_METADATA=1
#export CFLAGS+=-DSQLITE_TEMP_STORE=3 -DSQLITE_DEFAULT_MMAP_SIZE=0x10000000

# The build directory
BUILD_DIR=$(PLATFORM)/build

LIBDYLIB=$(BUILD_DIR)/lib$(LIBNAME).a

CMAKE_OPTIONS := \
  -DCMAKE_INSTALL_PREFIX=$(PREFIX) \
  -DYAML_CPP_BUILD_TESTS=OFF \
  -DYAML_CPP_BUILD_TOOLS=OFF \
  -DYAML_CPP_BUILD_CONTRIB=OFF \
  $(CMAKE_OPTIONS)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM)/*; mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)

$(LIBDYLIB): $(PLATFORM)
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR); $(CMAKE) $(CMAKE_OPTIONS) ..
	$(MAKE) -C $(BUILD_DIR)

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(BUILD_DIR) install
	touch $@

clean:
	$(MAKE) -C $(BUILD_DIR) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
