# Default value so one can compile i3-nagbar standalone
TOPDIR=..

include $(TOPDIR)/common.mk

CPPFLAGS += -I$(TOPDIR)/include

# Depend on the object files of all source-files in src/*.c and on all header files
FILES=$(patsubst %.c,%.o,$(wildcard *.c))
HEADERS=$(wildcard *.h)

# Depend on the specific file (.c for each .o) and on all headers
%.o: %.c ${HEADERS}
	echo "[i3-nagbar] CC $<"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

all: i3-nagbar

i3-nagbar: $(TOPDIR)/libi3/libi3.a ${FILES}
	echo "[i3-nagbar] LINK i3-nagbar"
	$(CC) $(LDFLAGS) -o $@ $(filter-out libi3/libi3.a,$^) $(LIBS)

$(TOPDIR)/libi3/%.a: $(TOPDIR)/libi3/*.c
	$(MAKE) -C $(TOPDIR)/libi3

install: all
	echo "[i3-nagbar] INSTALL"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 i3-nagbar $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f *.o

distclean: clean
	rm -f i3-nagbar
