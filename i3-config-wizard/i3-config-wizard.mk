ALL_TARGETS += i3-config-wizard/i3-config-wizard
INSTALL_TARGETS += install-i3-config-wizard
CLEAN_TARGETS += clean-i3-config-wizard

i3_config_wizard_SOURCES           := $(wildcard i3-config-wizard/*.c)
i3_config_wizard_HEADERS           := $(wildcard i3-config-wizard/*.h)
i3_config_wizard_CFLAGS             = $(XCB_CFLAGS) $(XCB_KBD_CFLAGS) $(PANGO_CFLAGS) $(XKB_COMMON_CFLAGS) $(XKB_COMMON_X11_CFLAGS)
i3_config_wizard_LIBS               = $(XCB_LIBS) $(XCB_KBD_LIBS) $(PANGO_LIBS) $(XKB_COMMON_LIBS) $(XKB_COMMON_X11_LIBS)

i3_config_wizard_OBJECTS := $(i3_config_wizard_SOURCES:.c=.o)


i3-config-wizard/%.o: i3-config-wizard/%.c $(i3_config_wizard_HEADERS) i3-config-parser.stamp
	echo "[i3-config-wizard] CC $<"
	$(CC) $(I3_CPPFLAGS) $(XCB_CPPFLAGS) $(CPPFLAGS) $(i3_config_wizard_CFLAGS) $(I3_CFLAGS) $(CFLAGS) -c -o $@ $<

i3-config-wizard/i3-config-wizard: libi3.a $(i3_config_wizard_OBJECTS)
	echo "[i3-config-wizard] Link i3-config-wizard"
	$(CC) $(I3_LDFLAGS) $(LDFLAGS) -o $@ $(filter-out libi3.a,$^) $(LIBS) $(i3_config_wizard_LIBS)

install-i3-config-wizard: i3-config-wizard/i3-config-wizard
	echo "[i3-config-wizard] Install"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(EXEC_PREFIX)/bin
	$(INSTALL) -m 0755 i3-config-wizard/i3-config-wizard $(DESTDIR)$(EXEC_PREFIX)/bin/

clean-i3-config-wizard:
	echo "[i3-config-wizard] Clean"
	rm -f $(i3_config_wizard_OBJECTS) $(i3_config_wizard_SOURCES_GENERATED) i3-config-wizard/i3-config-wizard i3-config-wizard/cfgparse.*
