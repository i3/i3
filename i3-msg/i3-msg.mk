ALL_TARGETS += i3-msg/i3-msg
INSTALL_TARGETS += install-i3-msg
CLEAN_TARGETS += clean-i3-msg

i3_msg_SOURCES := $(wildcard i3-msg/*.c)
i3_msg_HEADERS := $(wildcard i3-msg/*.h)
i3_msg_CFLAGS   = $(XCB_CFLAGS) $(PANGO_CFLAGS) $(YAJL_CFLAGS)
i3_msg_LIBS     = $(XCB_LIBS) $(YAJL_LIBS)

i3_msg_OBJECTS := $(i3_msg_SOURCES:.c=.o)


i3-msg/%.o: i3-msg/%.c $(i3_msg_HEADERS)
	echo "[i3-msg] CC $<"
	$(CC) $(I3_CPPFLAGS) $(XCB_CPPFLAGS) $(CPPFLAGS) $(i3_msg_CFLAGS) $(I3_CFLAGS) $(CFLAGS) -c -o $@ $<

i3-msg/i3-msg: libi3.a $(i3_msg_OBJECTS)
	echo "[i3-msg] Link i3-msg"
	$(CC) $(I3_LDFLAGS) $(LDFLAGS) -o $@ $(filter-out libi3.a,$^) $(LIBS) $(i3_msg_LIBS)

install-i3-msg: i3-msg/i3-msg
	echo "[i3-msg] Install"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(EXEC_PREFIX)/bin
	$(INSTALL) -m 0755 i3-msg/i3-msg $(DESTDIR)$(EXEC_PREFIX)/bin/

clean-i3-msg:
	echo "[i3-msg] Clean"
	rm -f $(i3_msg_OBJECTS) i3-msg/i3-msg
