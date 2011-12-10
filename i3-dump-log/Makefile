# Default value so one can compile i3-dump-log standalone
TOPDIR=..

include $(TOPDIR)/common.mk

CFLAGS += -I$(TOPDIR)/include

# Depend on the object files of all source-files in src/*.c and on all header files
FILES=$(patsubst %.c,%.o,$(wildcard *.c))
HEADERS=$(wildcard *.h)

# Depend on the specific file (.c for each .o) and on all headers
%.o: %.c ${HEADERS}
	echo "[i3-dump-log] CC $<"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

all: i3-dump-log

i3-dump-log: ${FILES}
	echo "[i3-dump-log] LINK i3-dump-log"
	$(CC) $(LDFLAGS) -o i3-dump-log ${FILES} $(LIBS)

install: all
	echo "[i3-dump-log] INSTALL"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 i3-dump-log $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f *.o

distclean: clean
	rm -f i3-dump-log
