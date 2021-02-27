include ../../Makefile.include
DEPS= ../../Makefile.include Makefile 16-kodi.conf

# lib name, version
LIBNAME=openssl
VERSION=1.1.1j
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

BASE_URL=https://www.openssl.org/source

# configuration settings
ifeq ($(OS), linux)
  TARGETOPT=--with-zlib-include=$(PREFIX)/include --with-zlib-lib=$(PREFIX)/lib
  OPENSSLPLATFORM=linux-$(MESON_CPU)
  # OpenSSL now uses pthread_atfork(), and this requires -lpthread on Linux to link against
  export LDFLAGS+= -lpthread
endif
ifeq ($(OS), android)
  TARGETOPT=--with-zlib-include=$(PREFIX)/include --with-zlib-lib=$(PREFIX)/lib -D__ANDROID_API__=$(NDK_LEVEL)
  # Openssl uses ANDROID_NDK_HOME in configure script.
  # requires toolchian path in $PATH, otherwise defaults to gcc which is being phased out in newer NDKs
  export ANDROID_NDK_HOME=$(NDKROOT)
  export PATH:=$(TOOLCHAIN)/bin:$(PATH)
  ifeq ($(MESON_CPU), aarch64)
    OPENSSLPLATFORM=android-arm64
  else
    OPENSSLPLATFORM=android-$(MESON_CPU)
  endif
endif
ifeq ($(OS), darwin_embedded)
  export SDKROOT CFLAGS
  OPENSSLPLATFORM=kodi-$(TARGET_PLATFORM)-$(CPU)
  ifeq ($(TARGET_PLATFORM),appletvos)
    # Need to add "no-async" to avoid "'setcontext' is unavailable: not available on tvOS" error
    TARGETOPT=no-async
  endif
endif
ifeq ($(OS), osx)
  OPENSSLPLATFORM=kodi-darwin64-$(CPU)
endif

CONFIGURE=./Configure $(OPENSSLPLATFORM) no-shared zlib no-tests $(TARGETOPT) --prefix=$(PREFIX)

LIBDYLIB=$(PLATFORM)/libssl.a

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); cp ../16-kodi.conf ./Configurations/
	cd $(PLATFORM); $(CONFIGURE)
	if test "$(OS)" = "darwin_embedded"; then \
		sed -E -ie "s|static volatile sig_atomic_t intr_signal;|static volatile intr_signal;|" "$(PLATFORM)/crypto/ui/ui_openssl.c"; \
	fi
	sed -ie "s|PROGRAMS=|PROGRAMS=#|" "$(PLATFORM)/Makefile";

$(LIBDYLIB): $(PLATFORM)
	$(MAKE) -C $(PLATFORM)
	touch $@

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM) install_sw
	touch $@

clean:
	$(MAKE) -C $(PLATFORM) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

