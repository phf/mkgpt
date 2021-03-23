CFLAGS:=-Wall -Wextra -Wpedantic -std=c11
ALL:=mkgpt

dev: CFLAGS+=-g3 -Og -fsanitize=address -fsanitize=undefined
dev: LDFLAGS+=-fsanitize=address -fsanitize=undefined
dev: $(ALL)

prod: CFLAGS+=-g0 -Os
prod: LDFLAGS+=-s
prod: $(ALL)

static: CC:=musl-gcc
static: CFLAGS+=-g0 -Os
static: LDFLAGS+=-static -s
static: $(ALL)

mkgpt: mkgpt.o crc32.o fstypes.o guid.o

mkgpt.o: mkgpt.c guid.h fstypes.h part.h
fstypes.o: fstypes.c fstypes.h guid.h
guid.o: guid.c guid.h
crc32.o: crc32.c

.PHONY: check clean format
check:
	cppcheck --enable=all --inconclusive --std=c11 .
clean:
	$(RM) *.o $(ALL)
format:
	clang-format -verbose -i *.[ch]
