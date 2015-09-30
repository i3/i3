ALL_TARGETS += i3-dump-log/i3-dump-log
INSTALL_TARGETS += install-i3-dump-log
CLEAN_TARGETS += clean-i3-dump-log

i3_dump_log_SOURCES := $(wildcard i3-dump-log/*.c)
i3_dump_log_HEADERS := $(wildcard i3-dump-log/*.h)
i3_dump_log_CFLAGS   = $(XCB_CFLAGS) $(PANGO_CFLAGS)
i3_dump_log_LIBS     = $(XCB_LIBS)

i3_dump_log_OBJECTS := $(i3_dump_log_SOURCES:.c=.o)


i3-dump-log/%.o: i3-dump-log/%.c $(i3_dump_log_HEADERS)
	echo "[i3-dump-log] CC $<"
	$(CC) $(I3_CPPFLAGS) $(XCB_CPPFLAGS) $(CPPFLAGS) $(i3_dump_log_CFLAGS) $(I3_CFLAGS) $(CFLAGS) -c -o $@ $<

i3-dump-log/i3-dump-log: libi3.a $(i3_dump_log_OBJECTS)
	echo "[i3-dump-log] Link i3-dump-log"
	$(CC) $(I3_LDFLAGS) $(LDFLAGS) -o $@ $(filter-out libi3.a,$^) $(LIBS) $(i3_dump_log_LIBS)

install-i3-dump-log: i3-dump-log/i3-dump-log
	echo "[i3-dump-log] Install"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(EXEC_PREFIX)/bin
	$(INSTALL) -m 0755 i3-dump-log/i3-dump-log $(DESTDIR)$(EXEC_PREFIX)/bin/

clean-i3-dump-log:
	echo "[i3-dump-log] Clean"
	rm -f $(i3_dump_log_OBJECTS) i3-dump-log/i3-dump-log
