ifeq (,$(wildcard config.mak))
$(error "config.mak is not present, run configure !")
endif
include config.mak

PKGCONFIG_DIR = $(libdir)/pkgconfig
PKGCONFIG_FILE = libvalhalla.pc

VHTEST = libvalhalla-test
VHTEST_SRCS = libvalhalla-test.c
VHTEST_OBJS = $(VHTEST_SRCS:.c=.o)

override CPPFLAGS += -Isrc
override LDFLAGS += -Lsrc -lvalhalla

ifeq ($(BUILD_STATIC),yes)
  override LDFLAGS += $(EXTRALIBS)
endif

DISTFILE = libvalhalla-$(VERSION).tar.bz2

EXTRADIST = \
	AUTHORS \
	ChangeLog \
	configure \
	COPYING \
	README \
	TODO \

SUBDIRS = \
	DOCS \
	src \
	utils \

.SUFFIXES: .c .o

all: lib apps docs

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(OPTFLAGS) -o $@ $<

lib:
	$(MAKE) -C src

$(VHTEST): $(VHTEST_OBJS)
	$(CC) $(VHTEST_OBJS) $(LDFLAGS) -o $(VHTEST)

apps-dep:
	$(CC) -MM $(CFLAGS) $(CPPFLAGS) $(VHTEST_SRCS) 1>.depend

apps: apps-dep lib
	$(MAKE) $(VHTEST)

docs:
	$(MAKE) -C DOCS

docs-clean:
	$(MAKE) -C DOCS clean

clean:
	$(MAKE) -C src clean
	rm -f *.o
	rm -f $(VHTEST)
	rm -f .depend

distclean: clean docs-clean
	rm -f config.log
	rm -f config.mak
	rm -f $(DISTFILE)
	rm -f $(PKGCONFIG_FILE)

install: install-lib install-pkgconfig install-apps install-docs

install-lib: lib
	$(MAKE) -C src install

install-pkgconfig: $(PKGCONFIG_FILE)
	$(INSTALL) -d "$(PKGCONFIG_DIR)"
	$(INSTALL) -m 644 $< "$(PKGCONFIG_DIR)"

install-apps: apps
	$(INSTALL) -d $(bindir)
	$(INSTALL) -c -m 755 $(VHTEST) $(bindir)

install-docs: docs
	$(MAKE) -C DOCS install

uninstall: uninstall-lib uninstall-pkgconfig uninstall-apps uninstall-docs

uninstall-lib:
	$(MAKE) -C src uninstall

uninstall-pkgconfig:
	rm -f $(PKGCONFIG_DIR)/$(PKGCONFIG_FILE)

uninstall-apps:
	rm -f $(bindir)/$(VHTEST)

uninstall-docs:
	$(MAKE) -C DOCS uninstall

.PHONY: *clean *install* docs apps*

dist:
	-$(RM) $(DISTFILE)
	dist=$(shell pwd)/libvalhalla-$(VERSION) && \
	for subdir in . $(SUBDIRS); do \
		mkdir -p "$$dist/$$subdir"; \
		$(MAKE) -C $$subdir dist-all DIST="$$dist/$$subdir"; \
	done && \
	tar cjf $(DISTFILE) libvalhalla-$(VERSION)
	-$(RM) -rf libvalhalla-$(VERSION)

dist-all:
	cp $(EXTRADIST) $(VHTEST_SRCS) Makefile $(DIST)

.PHONY: dist dist-all

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
