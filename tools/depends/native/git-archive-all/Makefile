include ../../Makefile.include
PLATFORM=$(NATIVEPLATFORM)
DEPS= ../../Makefile.include Makefile

# lib name, version
LIBNAME=git-archive-all
VERSION=1.23.0
SOURCE=$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

BASE_URL=https://github.com/Kentzo/git-archive-all/archive/refs/tags

LIBDYLIB=$(PLATFORM)/git_archive_all.py

ARCHIVE_TOOL_FLAGS=--strip-components=1 -xzf

all: $(TARBALLS_LOCATION)/$(ARCHIVE) $(LIBDYLIB) .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	cd $(TARBALLS_LOCATION); $(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM)/*; mkdir -p $(PLATFORM)
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)

$(LIBDYLIB): $(PLATFORM)
	touch "$(LIBDYLIB)"

.installed-$(PLATFORM): $(LIBDYLIB)
	cp $(LIBDYLIB) $(PREFIX)/bin
	touch $@

clean:
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
