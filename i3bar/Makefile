TOPDIR=..

include $(TOPDIR)/common.mk

FILES:=$(wildcard src/*.c)
FILES:=$(FILES:.c=.o)
HEADERS:=$(wildcard include/*.h)

CPPFLAGS += -I$(TOPDIR)/include

all: i3bar doc

i3bar: $(TOPDIR)/libi3/libi3.a ${FILES}
	echo "[i3bar] LINK"
	$(CC) $(LDFLAGS) -o $@ $(filter-out libi3/libi3.a,$^) $(LIBS)

libi3/%.a:
	$(MAKE) -C $(TOPDIR)/libi3

doc:
	echo ""
	echo "[i3bar] SUBDIR doc"
	$(MAKE) -C doc

src/%.o: src/%.c ${HEADERS}
	echo "[i3bar] CC $<"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

install: all
	echo "[i3bar] INSTALL"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 i3bar $(DESTDIR)$(PREFIX)/bin

clean:
	rm -f src/*.o
	$(MAKE) -C doc clean

distclean: clean
	rm -f i3bar
	$(MAKE) -C doc distclean

.PHONY: install clean distclean doc
