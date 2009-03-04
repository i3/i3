UNAME=$(shell uname)
DEBUG=1
INSTALL=install

CFLAGS += -std=c99
CFLAGS += -pipe
CFLAGS += -Wall
CFLAGS += -Wunused
CFLAGS += -Iinclude
CFLAGS += -I/usr/local/include

LDFLAGS += -lxcb-wm
#LDFLAGS += -lxcb-keysyms
LDFLAGS += -lxcb-xinerama
LDFLAGS += -lX11
LDFLAGS += -L/usr/local/lib -L/usr/pkg/lib
ifeq ($(UNAME),NetBSD)
CFLAGS += -I/usr/pkg/include
LDFLAGS += -Wl,-rpath,/usr/local/lib -Wl,-rpath,/usr/pkg/lib
endif

ifeq ($(UNAME),Linux)
CFLAGS += -D_GNU_SOURCE
endif

ifeq ($(DEBUG),1)
# Extended debugging flags, macros shall be available in gcc
CFLAGS += -gdwarf-2
CFLAGS += -g3
else
CFLAGS += -O2
endif

FILES=$(patsubst %.c,%.o,$(wildcard src/*.c))
HEADERS=$(wildcard include/*.h)

src/%.o: src/%.c ${HEADERS}
	$(CC) $(CFLAGS) -c -o $@ $<

all: ${FILES}
	$(CC) -o i3 ${FILES} $(LDFLAGS)

install: all
	$(INSTALL) -d -m 0755 $(DESTDIR)/usr/bin
	$(INSTALL) -d -m 0755 $(DESTDIR)/etc/i3
	$(INSTALL) -m 0755 i3 $(DESTDIR)/usr/bin/
	$(INSTALL) -m 0644 i3.config $(DESTDIR)/etc/i3/config

clean:
	rm -f src/*.o

distclean: clean
	rm -f i3
