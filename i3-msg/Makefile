# Default value so one can compile i3-msg standalone
TOPDIR=..

include $(TOPDIR)/common.mk

CFLAGS += -I$(TOPDIR)/include

# Depend on the object files of all source-files in src/*.c and on all header files
FILES=$(patsubst %.c,%.o,$(wildcard *.c))
HEADERS=$(wildcard *.h)

# Depend on the specific file (.c for each .o) and on all headers
%.o: %.c ${HEADERS}
	echo "CC $<"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

all: ${FILES}
	echo "LINK i3-msg"
	$(CC) -o i3-msg ${FILES} $(LDFLAGS)

install: all
	echo "INSTALL"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 i3-msg $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f *.o

distclean: clean
	rm -f i3-msg
