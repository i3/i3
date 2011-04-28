INSTALL=install
ifndef DEBUG
DEBUG=1
endif
PREFIX=/usr

# The escaping is absurd, but we need to escape for shell, sed, make, define
GIT_VERSION:="$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1), branch $(shell [ -f .git/HEAD ] && sed 's/ref: refs\/heads\/\(.*\)/\\\\\\"\1\\\\\\"/g' .git/HEAD || echo 'unknown'))"

# Fallback for libyajl 1 which did not include yajl_version.h. We need
# YAJL_MAJOR from that file to decide which code path should be used.
CFLAGS += -idirafter yajl-fallback

CFLAGS += -Wall
CFLAGS += -pipe
CFLAGS += -Iinclude
CFLAGS += -g
CFLAGS += -DI3BAR_VERSION=\"${GIT_VERSION}\"

LDFLAGS += -lev
LDFLAGS += -lyajl
LDFLAGS += -lxcb
LDFLAGS += -lX11
LDFLAGS += -L/usr/local/lib

ifeq ($(DEBUG),1)
CFLAGS += -g3
else
CFLAGS += -O2
endif

.SILENT:

.PHONY: install clean
