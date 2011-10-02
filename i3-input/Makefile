# Default value so one can compile i3-input standalone
TOPDIR=..

include $(TOPDIR)/common.mk

# Depend on the object files of all source-files in src/*.c and on all header files
FILES=$(patsubst %.c,%.o,$(wildcard *.c))
HEADERS=$(wildcard *.h)

# Depend on the specific file (.c for each .o) and on all headers
%.o: %.c ${HEADERS}
	echo "[i3-input] CC $<"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

all: i3-input

i3-input: ${FILES}
	echo "[i3-input] LINK i3-input"
	$(CC) $(LDFLAGS) -o $@ ${FILES} $(LIBS)

install: all
	echo "[i3-input] INSTALL"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 i3-input $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f *.o

distclean: clean
	rm -f i3-input
