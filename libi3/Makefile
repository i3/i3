# Default value so one can compile i3-msg standalone
TOPDIR=..

include $(TOPDIR)/common.mk

CFLAGS += -I$(TOPDIR)/include

# Depend on the object files of all source-files in src/*.c and on all header files
FILES=$(patsubst %.c,%.o,$(wildcard *.c))
HEADERS=$(wildcard *.h)

# Depend on the specific file (.c for each .o) and on all headers
%.o: %.c ${HEADERS}
	echo "[libi3] CC $<"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

all: libi3.a

libi3.a: ${FILES}
	echo "[libi3] AR libi3.a"
	ar rcs libi3.a ${FILES}

clean:
	rm -f *.o libi3.a

distclean: clean
