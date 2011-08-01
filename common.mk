UNAME=$(shell uname)
DEBUG=1
COVERAGE=0
INSTALL=install
ifndef PREFIX
  PREFIX=/usr
endif
ifndef SYSCONFDIR
  ifeq ($(PREFIX),/usr)
    SYSCONFDIR=/etc
  else
    SYSCONFDIR=$(PREFIX)/etc
  endif
endif
TERM_EMU=xterm
# The escaping is absurd, but we need to escape for shell, sed, make, define
GIT_VERSION:="$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1), branch $(shell [ -f $(TOPDIR)/.git/HEAD ] && sed 's/ref: refs\/heads\/\(.*\)/\\\\\\"\1\\\\\\"/g' $(TOPDIR)/.git/HEAD || echo 'unknown'))"
VERSION:=$(shell git describe --tags --abbrev=0)

ifeq ($(shell which pkg-config 2>/dev/null 1>/dev/null || echo 1),1)
$(error "pkg-config was not found")
endif

# An easier way to get CFLAGS and LDFLAGS falling back in case there's
# no pkg-config support for certain libraries
cflags_for_lib = $(shell pkg-config --silence-errors --cflags $(1))
ldflags_for_lib = $(shell pkg-config --exists $(1) && pkg-config --libs $(1) || echo -l$(2))

CFLAGS += -std=c99
CFLAGS += -pipe
CFLAGS += -Wall
# unused-function, unused-label, unused-variable are turned on by -Wall
# We don’t want unused-parameter because of the use of many callbacks
CFLAGS += -Wunused-value
CFLAGS += -Iinclude
CFLAGS += $(call cflags_for_lib, xcb-keysyms)
ifeq ($(shell pkg-config --exists xcb-util || echo 1),1)
CPPFLAGS += -DXCB_COMPAT
CFLAGS += $(call cflags_for_lib, xcb-atom)
CFLAGS += $(call cflags_for_lib, xcb-aux)
else
CFLAGS += $(call cflags_for_lib, xcb-util)
endif
CFLAGS += $(call cflags_for_lib, xcb-icccm)
CFLAGS += $(call cflags_for_lib, xcb-xinerama)
CFLAGS += $(call cflags_for_lib, xcb-randr)
CFLAGS += $(call cflags_for_lib, xcb)
CFLAGS += $(call cflags_for_lib, xcursor)
CFLAGS += $(call cflags_for_lib, x11)
CFLAGS += $(call cflags_for_lib, yajl)
CFLAGS += $(call cflags_for_lib, libev)
CPPFLAGS += -DI3_VERSION=\"${GIT_VERSION}\"
CPPFLAGS += -DSYSCONFDIR=\"${SYSCONFDIR}\"
CPPFLAGS += -DTERM_EMU=\"$(TERM_EMU)\"

LIBS += -lm
LIBS += $(call ldflags_for_lib, xcb-event, xcb-event)
LIBS += $(call ldflags_for_lib, xcb-keysyms, xcb-keysyms)
ifeq ($(shell pkg-config --exists xcb-util || echo 1),1)
LIBS += $(call ldflags_for_lib, xcb-atom, xcb-atom)
LIBS += $(call ldflags_for_lib, xcb-aux, xcb-aux)
else
LIBS += $(call ldflags_for_lib, xcb-util)
endif
LIBS += $(call ldflags_for_lib, xcb-icccm, xcb-icccm)
LIBS += $(call ldflags_for_lib, xcb-xinerama, xcb-xinerama)
LIBS += $(call ldflags_for_lib, xcb-randr, xcb-randr)
LIBS += $(call ldflags_for_lib, xcb, xcb)
LIBS += $(call ldflags_for_lib, xcursor, Xcursor)
LIBS += $(call ldflags_for_lib, x11, X11)
LIBS += $(call ldflags_for_lib, yajl, yajl)
LIBS += $(call ldflags_for_lib, libev, ev)

ifeq ($(UNAME),NetBSD)
# We need -idirafter instead of -I to prefer the system’s iconv over GNU libiconv
CFLAGS += -idirafter /usr/pkg/include
LDFLAGS += -Wl,-rpath,/usr/local/lib -Wl,-rpath,/usr/pkg/lib
endif

ifeq ($(UNAME),OpenBSD)
CFLAGS += -I${X11BASE}/include
LIBS += -liconv
LDFLAGS += -L${X11BASE}/lib
endif

ifeq ($(UNAME),FreeBSD)
LIBS += -liconv
endif

ifeq ($(UNAME),Darwin)
LIBS += -liconv
endif

# Fallback for libyajl 1 which did not include yajl_version.h. We need
# YAJL_MAJOR from that file to decide which code path should be used.
CFLAGS += -idirafter $(TOPDIR)/yajl-fallback

ifneq (,$(filter Linux GNU GNU/%, $(UNAME)))
CPPFLAGS += -D_GNU_SOURCE
endif

ifeq ($(DEBUG),1)
# Extended debugging flags, macros shall be available in gcc
CFLAGS += -gdwarf-2
CFLAGS += -g3
else
CFLAGS += -O2
CFLAGS += -freorder-blocks-and-partition
endif

ifeq ($(COVERAGE),1)
CFLAGS += -fprofile-arcs -ftest-coverage
LIBS += -lgcov
endif

# Don’t print command lines which are run
.SILENT:

# Always remake the following targets
.PHONY: install clean dist distclean

