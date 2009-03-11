# slock - simple screen locker
# © 2006-2007 Anselm R. Garbe, Sander van Dijk

include config.mk

SRC = slock.c
OBJ = ${SRC:.c=.o}

all: options slock

options:
	@echo slock build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

slock: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f slock ${OBJ} slock-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p slock-${VERSION}
	@cp -R LICENSE Makefile README config.mk ${SRC} slock-${VERSION}
	@tar -cf slock-${VERSION}.tar slock-${VERSION}
	@gzip slock-${VERSION}.tar
	@rm -rf slock-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f slock ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/slock
	@chmod u+s ${DESTDIR}${PREFIX}/bin/slock

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/slock

.PHONY: all options clean dist install uninstall
