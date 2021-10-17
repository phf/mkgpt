crc32.o: crc32.c crc32.h
guid.o: guid.c guid.h unaligned.h
mkgpt.o: mkgpt.c crc32.h guid.h part_ids.h unaligned.h
part_ids.o: part_ids.c part_ids.h guid.h
