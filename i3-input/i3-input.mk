ALL_TARGETS += i3-input/i3-input
INSTALL_TARGETS += install-i3-input
CLEAN_TARGETS += clean-i3-input

i3_input_SOURCES := $(wildcard i3-input/*.c)
i3_input_HEADERS := $(wildcard i3-input/*.h)
i3_input_CFLAGS   = $(XCB_CFLAGS) $(XCB_KBD_CFLAGS) $(PANGO_CFLAGS)
i3_input_LIBS     = $(XCB_LIBS) $(XCB_KBD_LIBS) $(PANGO_LIBS)

i3_input_OBJECTS := $(i3_input_SOURCES:.c=.o)


i3-input/%.o: i3-input/%.c $(i3_input_HEADERS)
	echo "[i3-input] CC $<"
	$(CC) $(I3_CPPFLAGS) $(XCB_CPPFLAGS) $(CPPFLAGS) $(i3_input_CFLAGS) $(I3_CFLAGS) $(CFLAGS) -c -o $@ $<

i3-input/i3-input: libi3.a $(i3_input_OBJECTS)
	echo "[i3-input] Link i3-input"
	$(CC) $(I3_LDFLAGS) $(LDFLAGS) -o $@ $(filter-out libi3.a,$^) $(LIBS) $(i3_input_LIBS)

install-i3-input: i3-input/i3-input
	echo "[i3-input] Install"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(EXEC_PREFIX)/bin
	$(INSTALL) -m 0755 i3-input/i3-input $(DESTDIR)$(EXEC_PREFIX)/bin/

clean-i3-input:
	echo "[i3-input] Clean"
	rm -f $(i3_input_OBJECTS) i3-input/i3-input
