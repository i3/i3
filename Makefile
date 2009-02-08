CFLAGS += -Wall
# Extended debugging flags, macros shall be available in gcc
CFLAGS += -gdwarf-2
CFLAGS += -g3
CFLAGS += -I/usr/include/xcb

LDFLAGS += -lxcb-wm

FILES=$(patsubst %.c,%.o,$(wildcard *.c))

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

all: ${FILES}
	$(CC) -o mainx ${FILES} $(LDFLAGS)

clean:
	rm -f *.o
