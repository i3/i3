CLEAN_TARGETS += clean-libi3

libi3_SOURCES := $(wildcard libi3/*.c)
libi3_HEADERS := $(wildcard libi3/*.h)
libi3_CFLAGS   =
libi3_LIBS     =

libi3_OBJECTS := $(libi3_SOURCES:.c=.o)


libi3/%.o: libi3/%.c $(libi3_HEADERS)
	echo "[libi3] CC $<"
	$(CC) $(CPPFLAGS) $(libi3_CFLAGS) $(CFLAGS) -c -o $@ $<

libi3.a: $(libi3_OBJECTS)
	echo "[libi3] AR libi3.a"
	ar rcs $@ $^ $(libi3_LIBS)

clean-libi3:
	echo "[libi3] Clean"
	rm -f $(libi3_OBJECTS) libi3.a
