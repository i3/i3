UNAME=$(shell uname)

CFLAGS += -Wall
# Extended debugging flags, macros shall be available in gcc
CFLAGS += -gdwarf-2
CFLAGS += -g3
CFLAGS += -I/usr/include/xcb
CFLAGS += -I/usr/local/include/
CFLAGS += -I/usr/local/include/xcb

LDFLAGS += -lxcb-wm
LDFLAGS += -L/usr/local/lib
ifeq ($(UNAME),NetBSD)
LDFLAGS += -Wl,-rpath,/usr/local/lib
endif

FILES=$(patsubst %.c,%.o,$(wildcard *.c))

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

all: ${FILES}
	$(CC) -o mainx ${FILES} $(LDFLAGS)

clean:
	rm -f *.o
