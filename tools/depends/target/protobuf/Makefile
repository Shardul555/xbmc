-include ../../Makefile.include
DEPS = ../../Makefile.include Makefile

# lib name, version
LIBNAME=protobuf
LANG=cpp
VERSION=3.7.1-p0
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

ifeq ($(PLATFORM),)
	# Building stand-alone
	ROOT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
	PLATFORM = native
	TARBALLS_LOCATION = $(ROOT_DIR)
	BASE_URL := http://mirrors.kodi.tv/build-deps/sources
	RETRIEVE_TOOL := curl -Ls --create-dirs -f -O
	ARCHIVE_TOOL := tar --strip-components=1 -xf
else
	# Building as part of depends
	DEPS += ../../Makefile.include
	CONFIGURE_OPTIONS := \
	  --with-protoc=$(NATIVEPREFIX)/bin/protoc \
	  $(CONFIGURE_OPTIONS)
endif

# TODO
GIT_URL=https://github.com/hunter-packages/protobuf.git

# The build directory
BUILD_DIR=$(PLATFORM)/build

LIBDYLIB=$(BUILD_DIR)/lib$(LIBNAME)3.a

CMAKE_OPTIONS := \
  -DCMAKE_INSTALL_PREFIX=$(PREFIX) \
  -Dprotobuf_BUILD_TESTS=OFF \
  -Dprotobuf_BUILD_PROTOC_BINARIES=OFF \
  -Dprotobuf_BUILD_SHARED_LIBS=OFF \
  -DHUNTER_ENABLED=OFF \
  $(CMAKE_OPTIONS)

# Fix missing log dependency when linking '__android_log_write'
# https://github.com/protocolbuffers/protobuf/issues/2719
ifeq ($(OS),android)
	LDFLAGS := $(LDFLAGS) -llog
endif

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	rm -rf $(TARBALLS_LOCATION)/$(LIBNAME)
	git clone $(GIT_URL) $(TARBALLS_LOCATION)/$(LIBNAME) -b v$(VERSION)
	python3 $(PREFIX)/bin/git_archive_all.py -C $(TARBALLS_LOCATION)/$(LIBNAME) --force-submodules $(TARBALLS_LOCATION)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
ifeq ($(PREFIX),)
	@echo
	@echo "ERROR: please set PREFIX to the kodi install path e.g. make PREFIX=/usr/local"
	@exit 1
endif
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)

$(LIBDYLIB): $(PLATFORM)
	mkdir -p "$(BUILD_DIR)"
	cd "$(BUILD_DIR)"; $(CMAKE) $(CMAKE_OPTIONS) ..
	$(MAKE) -C $(BUILD_DIR)

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(BUILD_DIR) install
	touch $@

clean:
	$(MAKE) -C $(BUILD_DIR) clean
	rm -f .installed-$(PLATFORM)

distclean:
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
