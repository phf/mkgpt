.POSIX:
CC?=cc # GNU make .POSIX fix
SHELL=/bin/sh # paranoia

.PHONY: prod static dev sane  check clean depend format  install uninstall
.SUFFIXES:
.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

CFLAGS+=-Wall -Wextra -Wpedantic -std=c11 -D_DEFAULT_SOURCE #-D_FORTIFY_SOURCE=2
LDFLAGS+=

OBJS=mkgpt.o crc32.o guid.o part_ids.o

mkgpt: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

# use "make depend" to generate a new one
-include deps.mk

# different ways to build
prod:
	CFLAGS="-g0 -Os" LDFLAGS="-s" $(MAKE) mkgpt
static:
	CFLAGS="-g0 -Os" LDFLAGS="-s -static" $(MAKE) mkgpt
dev:
	CFLAGS="-g3 -Og" $(MAKE) mkgpt
sane:
	CFLAGS="-g3 -Og -fsanitize=address -fsanitize=undefined" \
	LDFLAGS="-fsanitize=address -fsanitize=undefined" \
	$(MAKE) mkgpt

check:
	-cppcheck --enable=all --inconclusive --std=c11 .
	-shellcheck *.sh
clean:
	rm -fv $(OBJS) mkgpt
depend:
	$(CC) -MM *.c >deps.mk
format:
	-clang-format -verbose -i *.[ch]

PREFIX=/usr/local
install:
	mkdir -pv $(DESTDIR)$(PREFIX)/bin
	cp -fv mkgpt $(DESTDIR)$(PREFIX)/bin
uninstall:
	rm -fv $(DESTDIR)$(PREFIX)/bin/mkgpt
