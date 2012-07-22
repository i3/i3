ALL_TARGETS += i3-config-wizard/i3-config-wizard
INSTALL_TARGETS += install-i3-config-wizard
CLEAN_TARGETS += clean-i3-config-wizard

i3_config_wizard_SOURCES_GENERATED  = i3-config-wizard/cfgparse.tab.c i3-config-wizard/cfgparse.yy.c
i3_config_wizard_SOURCES           := $(filter-out $(i3_config_wizard_SOURCES_GENERATED),$(wildcard i3-config-wizard/*.c))
i3_config_wizard_HEADERS           := $(wildcard i3-config-wizard/*.h)
i3_config_wizard_CFLAGS             =
i3_config_wizard_LIBS               =

i3_config_wizard_OBJECTS := $(i3_config_wizard_SOURCES_GENERATED:.c=.o) $(i3_config_wizard_SOURCES:.c=.o)


i3-config-wizard/%.o: i3-config-wizard/%.c $(i3_config_wizard_HEADERS)
	echo "[i3-config-wizard] CC $<"
	$(CC) $(CPPFLAGS) $(i3_config_wizard_CFLAGS) $(CFLAGS) -c -o $@ $<

i3-config-wizard/cfgparse.yy.c: i3-config-wizard/cfgparse.l i3-config-wizard/cfgparse.tab.o $(i3_config_wizard_HEADERS)
	echo "[i3-config-wizard] LEX $<"
	$(FLEX) -i -o $@ $<

i3-config-wizard/cfgparse.tab.c: i3-config-wizard/cfgparse.y $(i3_config_wizard_HEADERS)
	echo "[i3-config-wizard] YACC $<"
	$(BISON) --debug --verbose -b $(basename $< .y) -d $<

i3-config-wizard/i3-config-wizard: libi3.a $(i3_config_wizard_OBJECTS)
	echo "[i3-config-wizard] Link i3-config-wizard"
	$(CC) $(LDFLAGS) -o $@ $(filter-out libi3.a,$^) $(i3_config_wizard_LIBS) $(LIBS)

install-i3-config-wizard: i3-config-wizard/i3-config-wizard
	echo "[i3-config-wizard] Install"
	$(INSTALL) -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 i3-config-wizard/i3-config-wizard $(DESTDIR)$(PREFIX)/bin/

clean-i3-config-wizard:
	echo "[i3-config-wizard] Clean"
	rm -f $(i3_config_wizard_OBJECTS) $(i3_config_wizard_SOURCES_GENERATED) i3-config-wizard/i3-config-wizard i3-config-wizard/cfgparse.{output,dot}
