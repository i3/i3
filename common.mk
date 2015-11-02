UNAME=$(shell uname)
DEBUG=1
INSTALL=install
LN=ln
PKG_CONFIG=pkg-config
ifndef PREFIX
  PREFIX=/usr
endif
ifndef EXEC_PREFIX
  EXEC_PREFIX=$(PREFIX)
endif
ifndef SYSCONFDIR
  ifeq ($(PREFIX),/usr)
    SYSCONFDIR=/etc
  else
    SYSCONFDIR=$(PREFIX)/etc
  endif
endif

# In dist tarballs, the version is stored in the I3_VERSION and VERSION files.
I3_VERSION := '$(shell [ -f $(TOPDIR)/I3_VERSION ] && cat $(TOPDIR)/I3_VERSION)'
VERSION := '$(shell [ -f $(TOPDIR)/VERSION ] && cat $(TOPDIR)/VERSION)'
ifeq ('',$(I3_VERSION))
VERSION := $(shell git describe --tags --abbrev=0)
I3_VERSION := '$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1), branch \"$(shell git describe --tags --always --all | sed s:heads/::)\")'
endif

MAJOR_VERSION := $(shell echo ${VERSION} | cut -d '.' -f 1)
MINOR_VERSION := $(shell echo ${VERSION} | cut -d '.' -f 2)
PATCH_VERSION := $(shell echo ${VERSION} | cut -d '.' -f 3)
ifeq (${PATCH_VERSION},)
PATCH_VERSION := 0
endif

## Generic flags

# Default CFLAGS that users should be able to override
ifeq ($(DEBUG),1)
# Extended debugging flags, macros shall be available in gcc
CFLAGS ?= -pipe -gdwarf-2 -g3
else
CFLAGS ?= -pipe -O2 -freorder-blocks-and-partition
endif

# Default LDFLAGS that users should be able to override
LDFLAGS ?= $(as_needed_LDFLAG)

# Common CFLAGS for all i3 related binaries
I3_CFLAGS  = -std=c99
I3_CFLAGS += -Wall
# unused-function, unused-label, unused-variable are turned on by -Wall
# We don’t want unused-parameter because of the use of many callbacks
I3_CFLAGS += -Wunused-value
I3_CFLAGS += -Iinclude

I3_CPPFLAGS  = -DI3_VERSION=\"${I3_VERSION}\"
I3_CPPFLAGS += -DMAJOR_VERSION=${MAJOR_VERSION}
I3_CPPFLAGS += -DMINOR_VERSION=${MINOR_VERSION}
I3_CPPFLAGS += -DPATCH_VERSION=${PATCH_VERSION}
I3_CPPFLAGS += -DSYSCONFDIR=\"${SYSCONFDIR}\"
I3_CPPFLAGS += -DI3__FILE__=__FILE__


## Libraries flags

ifeq ($(shell which $(PKG_CONFIG) 2>/dev/null 1>/dev/null || echo 1),1)
$(error "pkg-config was not found")
endif

# An easier way to get CFLAGS and LDFLAGS falling back in case there's
# no pkg-config support for certain libraries.
#
# NOTE that you must not use a blank after comma when calling this:
#     $(call ldflags_for_lib name, fallback) # bad
#     $(call ldflags_for_lib name,fallback) # good
# Otherwise, the compiler will get -l foo instead of -lfoo
#
# We redirect stderr to /dev/null because pkg-config prints an error if support
# for gnome-config was enabled but gnome-config is not actually installed.
cflags_for_lib = $(shell $(PKG_CONFIG) --silence-errors --cflags $(1) 2>/dev/null)
ldflags_for_lib = $(shell $(PKG_CONFIG) --exists 2>/dev/null $(1) && $(PKG_CONFIG) --libs $(1) 2>/dev/null || echo -l$(2))

# XCB common stuff
XCB_CFLAGS  := $(call cflags_for_lib, xcb)
XCB_CFLAGS  += $(call cflags_for_lib, xcb-event)
XCB_LIBS    := $(call ldflags_for_lib, xcb,xcb)
XCB_LIBS    += $(call ldflags_for_lib, xcb-event,xcb-event)
ifeq ($(shell $(PKG_CONFIG) --exists xcb-util 2>/dev/null || echo 1),1)
XCB_CFLAGS  += $(call cflags_for_lib, xcb-atom)
XCB_CFLAGS  += $(call cflags_for_lib, xcb-aux)
XCB_LIBS    += $(call ldflags_for_lib, xcb-atom,xcb-atom)
XCB_LIBS    += $(call ldflags_for_lib, xcb-aux,xcb-aux)
XCB_CPPFLAGS+= -DXCB_COMPAT
else
XCB_CFLAGS  += $(call cflags_for_lib, xcb-util)
XCB_LIBS    += $(call ldflags_for_lib, xcb-util)
endif
XCB_XKB_LIBS := $(call ldflags_for_lib, xcb-xkb,xcb-xkb)

# XCB keyboard stuff
XCB_KBD_CFLAGS := $(call cflags_for_lib, xcb-keysyms)
XCB_KBD_LIBS   := $(call ldflags_for_lib, xcb-keysyms,xcb-keysyms)

# XCB WM stuff
XCB_WM_CFLAGS := $(call cflags_for_lib, xcb-icccm)
XCB_WM_CFLAGS += $(call cflags_for_lib, xcb-xinerama)
XCB_WM_CFLAGS += $(call cflags_for_lib, xcb-randr)
XCB_WM_LIBS   := $(call ldflags_for_lib, xcb-icccm,xcb-icccm)
XCB_WM_LIBS   += $(call ldflags_for_lib, xcb-xinerama,xcb-xinerama)
XCB_WM_LIBS   += $(call ldflags_for_lib, xcb-randr,xcb-randr)

XKB_COMMON_CFLAGS := $(call cflags_for_lib, xkbcommon,xkbcommon)
XKB_COMMON_LIBS := $(call ldflags_for_lib, xkbcommon,xkbcommon)
XKB_COMMON_X11_CFLAGS := $(call cflags_for_lib, xkbcommon-x11,xkbcommon-x11)
XKB_COMMON_X11_LIBS := $(call ldflags_for_lib, xkbcommon-x11,xkbcommon-x11)

# Xcursor
XCURSOR_CFLAGS := $(call cflags_for_lib, xcb-cursor)
XCURSOR_LIBS   := $(call ldflags_for_lib, xcb-cursor,xcb-cursor)

# yajl
YAJL_CFLAGS := $(call cflags_for_lib, yajl)
YAJL_LIBS   := $(call ldflags_for_lib, yajl,yajl)

#libev
LIBEV_CFLAGS := $(call cflags_for_lib, libev)
LIBEV_LIBS   := $(call ldflags_for_lib, libev,ev)

# libpcre
PCRE_CFLAGS := $(call cflags_for_lib, libpcre)
ifeq ($(shell $(PKG_CONFIG) --atleast-version=8.10 libpcre 2>/dev/null && echo 1),1)
I3_CPPFLAGS += -DPCRE_HAS_UCP=1
endif
PCRE_LIBS   := $(call ldflags_for_lib, libpcre,pcre)

# startup-notification
LIBSN_CFLAGS := $(call cflags_for_lib, libstartup-notification-1.0)
LIBSN_LIBS   := $(call ldflags_for_lib, libstartup-notification-1.0,startup-notification-1)

# Pango
PANGO_CFLAGS := $(call cflags_for_lib, cairo)
PANGO_CFLAGS += $(call cflags_for_lib, pangocairo)
I3_CPPFLAGS  += -DPANGO_SUPPORT=1
ifeq ($(shell $(PKG_CONFIG) --atleast-version=1.14.4 cairo 2>/dev/null && echo 1),1)
I3_CPPFLAGS  += -DI3BAR_CAIRO=1
endif
PANGO_LIBS   := $(call ldflags_for_lib, cairo)
PANGO_LIBS   += $(call ldflags_for_lib, pangocairo)

# libi3
LIBS = -L$(TOPDIR) -li3 -lm

## Platform-specific flags

# Please test if -Wl,--as-needed works on your platform and send me a patch.
# it is known not to work on Darwin (Mac OS X)
ifneq (,$(filter Linux GNU GNU/%, $(UNAME)))
as_needed_LDFLAG = -Wl,--as-needed
endif

ifeq ($(UNAME),NetBSD)
# We need -idirafter instead of -I to prefer the system’s iconv over GNU libiconv
I3_CFLAGS += -idirafter /usr/pkg/include
I3_LDFLAGS += -Wl,-rpath,/usr/local/lib -Wl,-rpath,/usr/pkg/lib
endif

ifeq ($(UNAME),OpenBSD)
I3_CFLAGS += -I${X11BASE}/include
LIBS += -liconv
I3_LDFLAGS += -L${X11BASE}/lib
endif

ifeq ($(UNAME),FreeBSD)
LIBS += -liconv
endif

ifeq ($(UNAME),Darwin)
LIBS += -liconv
else ifneq ($(UNAME),OpenBSD)
# Darwin (Mac OS X) and OpenBSD do not have librt
LIBS += -lrt
endif

ifeq ($(UNAME),SunOS)
LIBS += -lsocket -liconv -lgen
endif

ifneq (,$(filter Linux GNU GNU/%, $(UNAME)))
I3_CPPFLAGS += -D_GNU_SOURCE
endif


ifeq ($(COVERAGE),1)
I3_CFLAGS += -fprofile-arcs -ftest-coverage
LIBS += -lgcov
endif

V ?= 0
ifeq ($(V),0)
# Don’t print command lines which are run
.SILENT:

# echo-ing vars
V_ASCIIDOC = echo ASCIIDOC $@;
V_POD2HTML = echo POD2HTML $@;
V_POD2MAN = echo POD2MAN $@;
V_A2X = echo A2X $@;
endif

# Always remake the following targets
.PHONY: install clean dist distclean

