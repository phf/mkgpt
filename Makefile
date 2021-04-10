CFLAGS+=-Wall -Wextra -Wpedantic -std=c11 -D_DEFAULT_SOURCE
ALL:=mkgpt
PREFIX:=/usr/local

dev: CFLAGS+=-g3 -Og -fsanitize=address -fsanitize=undefined
dev: LDFLAGS+=-fsanitize=address -fsanitize=undefined
dev: $(ALL)

prod: CFLAGS+=-g0 -Os
prod: LDFLAGS+=-s
prod: $(ALL)

static: LDFLAGS+=-static
static: prod

musl-static: CC:=musl-gcc
musl-static: static

mkgpt: mkgpt.o crc32.o fstypes.o guid.o

mkgpt.o: mkgpt.c guid.h fstypes.h part.h crc32.h
fstypes.o: fstypes.c fstypes.h guid.h
guid.o: guid.c guid.h
crc32.o: crc32.c crc32.h

.PHONY: check clean format install uninstall
check:
	-cppcheck --enable=all --inconclusive --std=c11 .
	-shellcheck *.sh
clean:
	$(RM) *.o $(ALL)
format:
	-clang-format -verbose -i *.[ch]
install: prod
	mkdir -pv $(DESTDIR)$(PREFIX)/bin
	cp -fv mkgpt $(DESTDIR)$(PREFIX)/bin
uninstall:
	rm -fv $(DESTDIR)$(PREFIX)/bin/mkgpt
