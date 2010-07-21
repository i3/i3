TOPDIR=$(shell pwd)

include $(TOPDIR)/common.mk

FILES:=$(wildcard src/*.c)
FILES:=$(FILES:.c=.o)
HEADERS:=$(wildcard include/*.h)

all: ${FILES}
	echo "LINK"
	$(CC) -o i3bar ${FILES} ${LDFLAGS}

src/%.o: src/%.c ${HEADERS}
	echo "CC $<"
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm src/*.o
