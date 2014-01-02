ALL_TARGETS += i3bar/i3bar
INSTALL_TARGETS += install-i3bar
CLEAN_TARGETS += clean-i3bar

i3bar_SOURCES := $(wildcard i3bar/src/*.c)
i3bar_HEADERS := $(wildcard i3bar/include/*.h)
i3bar_CFLAGS   = $(XCB_CFLAGS) $(PANGO_CFLAGS) $(YAJL_CFLAGS) $(LIBEV_CFLAGS)
i3bar_LIBS     = $(XCB_LIBS) $(PANGO_LIBS) $(YAJL_LIBS) $(LIBEV_LIBS) $(XCB_XKB_LIBS)

i3bar_OBJECTS := $(i3bar_SOURCES:.c=.o)


i3bar/src/%.o: i3bar/src/%.c $(i3bar_HEADERS)
	echo "[i3bar] CC $<"
	$(CC) $(I3_CPPFLAGS) $(XCB_CPPFLAGS) $(CPPFLAGS) $(i3bar_CFLAGS) $(I3_CFLAGS) $(CFLAGS) -Ii3bar/include -c -o $@ $<

i3bar/i3bar: libi3.a $(i3bar_OBJECTS)
	echo "[i3bar] Link i3bar"
	$(CC) $(I3_LDFLAGS) $(LDFLAGS) -o $@ $(filter-out libi3.a,$^) $(LIBS) $(i3bar_LIBS)

install-i3bar: i3bar/i3bar
	echo "[i3bar] Install"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 i3bar/i3bar $(DESTDIR)$(PREFIX)/bin/

clean-i3bar:
	echo "[i3bar] Clean"
	rm -f $(i3bar_OBJECTS) i3bar/i3bar
