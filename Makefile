UNAME=$(shell uname)

CFLAGS += -Wall
# Extended debugging flags, macros shall be available in gcc
CFLAGS += -gdwarf-2
CFLAGS += -g3
#CFLAGS += -I/usr/include/xcb
CFLAGS += -I/usr/local/include/
#CFLAGS += -I/usr/local/include/xcb
CFLAGS += -I/usr/pkg/include

LDFLAGS += -lxcb-wm
LDFLAGS += -lxcb-keysyms
LDFLAGS += -lxcb-xinerama
LDFLAGS += -lX11
LDFLAGS += -L/usr/local/lib -L/usr/pkg/lib
ifeq ($(UNAME),NetBSD)
LDFLAGS += -Wl,-rpath,/usr/local/lib -Wl,-rpath,/usr/pkg/lib
endif

FILES=$(patsubst %.c,%.o,$(wildcard *.c))

%.o: %.c %.h data.h
	$(CC) $(CFLAGS) -c -o $@ $<

all: ${FILES}
	$(CC) -o mainx ${FILES} $(LDFLAGS)

clean:
	rm -f *.o
