TOPDIR=..

include $(TOPDIR)/common.mk

FILES:=$(wildcard src/*.c)
FILES:=$(FILES:.c=.o)
HEADERS:=$(wildcard include/*.h)

CPPFLAGS += -I$(TOPDIR)/include

all: i3bar doc

i3bar: ${FILES}
	echo "LINK"
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

doc:
	echo ""
	echo "SUBDIR doc"
	$(MAKE) -C doc

src/%.o: src/%.c ${HEADERS}
	echo "CC $<"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

install: all
	echo "INSTALL"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 i3bar $(DESTDIR)$(PREFIX)/bin

clean:
	rm -f src/*.o
	make -C doc clean

distclean: clean
	rm -f i3bar
	make -C doc distclean

.PHONY: install clean distclean doc
