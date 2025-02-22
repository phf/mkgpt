/* SPDX-License-Identifier: MIT */

/* Copyright (C) 2014 by John Cronin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "crc32.h"
#include "guid.h"
#include "part_ids.h"
#include "unaligned.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct partition {
	GUID type;
	GUID uuid;
	uint64_t attrs;
	long src_length;
	FILE *src;
	struct partition *next; /* TODO why build a list? */
	int id;
	int sect_start;
	int sect_length;
	char name[52];
};

#define MAX_PART_NAME (36U)
#define MIN_SECTOR_SIZE (512U)
#define MAX_SECTOR_SIZE (4096U)

/*
 * UEFI says 128 is the "minimum size" but since we're generating the image we
 * get to pick; and we're fine with 128 for now; anything else would probably
 * also mess with other GPT tools?
 */
#define PART_ENTRY_SIZE (128U)

/*
 * TODO Everything else says 92 instead, and that's also what gdisk does when
 * it creates a GPT. It's a mystery why the code here uses 96 instead.
 */
#define GPT_HEADER_SIZE (96U)

static void
dump_help(char *fname);
static int
check_parts();
static int
parse_opts(int argc, char **argv);
static void
write_output();

static inline int
min(const int a, const int b)
{
	return a < b ? a : b;
}

static size_t sect_size = MIN_SECTOR_SIZE;
static long image_sects = 0;
static long min_image_sects = 2048;
static struct partition *first_part = NULL;
static struct partition *last_part = NULL;
static FILE *output = NULL;
static GUID disk_guid;
static int part_count;
static int header_sectors;
static int first_usable_sector;
static int secondary_headers_sect;
static int secondary_gpt_sect;

int
main(int argc, char *argv[])
{
#if defined(__OpenBSD__)
	if (pledge("stdio cpath rpath wpath", NULL) != 0) {
		fprintf(stderr, "failed to pledge\n");
		exit(EXIT_FAILURE);
	}
	/* TODO call unveil on each path AHEAD of using it? */
#endif

	random_guid(&disk_guid);

	if (parse_opts(argc, argv) != 0) {
		exit(EXIT_FAILURE);
	}

	if (output == NULL) {
		fprintf(stderr, "no output file specified\n");
		dump_help(argv[0]);
		exit(EXIT_FAILURE);
	}
	if (first_part == NULL) {
		fprintf(stderr, "no partitions specified\n");
		dump_help(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (check_parts() != 0) {
		exit(EXIT_FAILURE);
	}

	write_output();
	fclose(output);

	exit(EXIT_SUCCESS);
}

static int
parse_opts(int argc, char *argv[])
{
	int i = 1;
	int cur_part_id = 0;
	struct partition *cur_part = NULL;

	/* First, parse global options */
	while (i < argc) {
		if (!strcmp(argv[i], "--output") || !strcmp(argv[i], "-o")) {
			i++;
			if (i == argc || argv[i][0] == '-') {
				fprintf(stderr, "no output file specified\n");
				return -1;
			}

			output = fopen(argv[i], "w+");
			if (output == NULL) {
				fprintf(stderr,
					"unable to open %s for writing (%s)\n",
					argv[i], strerror(errno));
				return -1;
			}
			i++;
		} else if (!strcmp(argv[i], "--disk-guid")) {
			i++;
			if (i == argc || argv[i][0] == '-') {
				fprintf(stderr,
					"no disk guid file specified\n");
				return -1;
			}

			if (parse_guid(argv[i], &disk_guid) != 0) {
				fprintf(stderr, "invalid disk uuid (%s)\n",
					argv[i]);
				return -1;
			}

			i++;
		} else if (!strcmp(argv[i], "--help") ||
			   !strcmp(argv[i], "-h")) {
			dump_help(argv[0]);
			return -1;
		} else if (!strcmp(argv[i], "--sector-size")) {
			i++;
			if (i == argc || argv[i][0] == '-') {
				fprintf(stderr, "sector size not specified\n");
				return -1;
			}

			sect_size = atoi(argv[i]);

			if (sect_size < MIN_SECTOR_SIZE ||
				sect_size > MAX_SECTOR_SIZE ||
				sect_size % MIN_SECTOR_SIZE) {
				fprintf(stderr,
					"invalid sector size (%zu) - must be "
					">= %u and <= %u and "
					"a multiple of %u",
					sect_size, MIN_SECTOR_SIZE,
					MAX_SECTOR_SIZE, MIN_SECTOR_SIZE);
				return -1;
			}
			i++;
		} else if (!strcmp(argv[i], "--minimum-image-size") ||
			   !strcmp(argv[i], "-s")) {
			i++;
			if (i == argc || argv[i][0] == '-') {
				fprintf(stderr,
					"minimum image size not specified\n");
				return -1;
			}

			min_image_sects = atoi(argv[i]);

			if (min_image_sects < 2048) {
				fprintf(stderr, "minimum image size must be at "
						"least 2048 sectors\n");
				return -1;
			}

			i++;
		} else if (!strcmp(argv[i], "--image-size")) {
			i++;
			if (i == argc || argv[i][0] == '-') {
				fprintf(stderr, "image size not specified\n");
				return -1;
			}

			image_sects = atoi(argv[i]);

			i++;
		} else if (!strcmp(argv[i], "--part") || !strcmp(argv[i], "-p"))
			break;
		else {
			fprintf(stderr, "unknown argument - %s\n", argv[i]);
			dump_help(argv[0]);
			return i;
		}
	}

	/* Now parse partitions */
	while (i < argc) {
		if (!strcmp(argv[i], "--part") || !strcmp(argv[i], "-p")) {
			/* Store the current partition data if there is one */
			if (cur_part != NULL) {
				if (last_part == NULL) {
					first_part = last_part = cur_part;
					cur_part->next = NULL;
				} else {
					last_part->next = cur_part;
					last_part = cur_part;
					cur_part->next = NULL;
				}
			}

			/* Allocate a new partition structure */
			cur_part = calloc(1, sizeof(*cur_part));
			if (cur_part == NULL) {
				fprintf(stderr, "out of memory allocating "
						"partition structure\n");
				return -1;
			}
			cur_part_id++;
			cur_part->id = cur_part_id;
			snprintf(cur_part->name, sizeof(cur_part->name),
				"part%i", cur_part_id);

			/* Get the filename of the partition image */
			i++;
			if (i == argc || argv[i][0] == '-') {
				fprintf(stderr,
					"no partition image specified for "
					"partition %i\n",
					cur_part_id);
				return -1;
			}
			cur_part->src = fopen(argv[i], "r");
			if (cur_part->src == NULL) {
				fprintf(stderr,
					"unable to open partition image (%s) "
					"for partition (%i) - %s\n",
					argv[i], cur_part_id, strerror(errno));
				return -1;
			}

			i++;
		} else if (!strcmp(argv[i], "--name") ||
			   !strcmp(argv[i], "-n")) {
			if (cur_part == NULL) {
				fprintf(stderr, "--part must be specified "
						"before --name argument\n");
				return -1;
			}

			i++;
			if (i == argc || argv[i][0] == '-') {
				fprintf(stderr,
					"partition name not specified %i\n",
					cur_part_id);
				return -1;
			}

			/*
			 * TODO we would really need to check the number of
			 * UTF-8 characters and not the number of bytes here...
			 */
			if (strlen(argv[i]) > MAX_PART_NAME) {
				fprintf(stderr,
					"partition name too long (max %u)\n",
					MAX_PART_NAME);
				return -1;
			}
			static_assert(sizeof(cur_part->name) >= MAX_PART_NAME,
				"more space for name in struct partition");
			strcpy(cur_part->name, argv[i]);

			i++;
		} else if (!strcmp(argv[i], "--type") ||
			   (!strcmp(argv[i], "-t"))) {
			if (cur_part == NULL) {
				fprintf(stderr, "--part must be specifed "
						"before --type argument\n");
				return -1;
			}

			i++;
			if (i == argc || argv[i][0] == '-') {
				fprintf(stderr,
					"partition type not specified %i\n",
					cur_part_id);
				return -1;
			}

			if (parse_guid(argv[i], &cur_part->type) != 0) {
				fprintf(stderr,
					"invalid partition type (%s) for "
					"partition %i\n",
					argv[i], cur_part_id);
				return -1;
			}

			i++;
		} else if (!strcmp(argv[i], "--uuid") ||
			   (!strcmp(argv[i], "-u"))) {
			if (cur_part == NULL) {
				fprintf(stderr, "--part must be specifed "
						"before --uuid argument\n");
				return -1;
			}

			i++;
			if (i == argc || argv[i][0] == '-') {
				fprintf(stderr,
					"partition uuid not specified %i\n",
					cur_part_id);
				return -1;
			}

			if (parse_guid(argv[i], &cur_part->uuid) != 0) {
				fprintf(stderr,
					"invalid partition uuid (%s) for "
					"partition %i\n",
					argv[i], cur_part_id);
				return -1;
			}

			i++;
		} else {
			fprintf(stderr, "unknown argument - %s\n", argv[i]);
			dump_help(argv[0]);
			return i;
		}
	}

	if (cur_part != NULL) {
		if (last_part == NULL) {
			first_part = last_part = cur_part;
			cur_part->next = NULL;
		} else {
			last_part->next = cur_part;
			last_part = cur_part;
			cur_part->next = NULL;
		}
	}

	return 0;
}

static void
dump_help(char *fname)
{
	printf("Usage: %s -o <output_file> [-h] [--disk-guid GUID] "
	       "[--sector-size sect_size] [-s min_image_size] "
	       "[partition def 0] [part def 1] ... [part def n]\n"
	       "  Partition definition: --part <image_file> --type <type> "
	       "[--uuid uuid] [--name name]\n"
	       "  Please see the README file for further information\n",
		fname);
}

static int
check_parts()
{
	/* Iterate through the partitions, checking validity */
	int cur_part_id = 0;
	int cur_sect;
	struct partition *cur_part;
	int header_length;
	int needed_file_length;

	/* Count partitions */
	cur_part = first_part;
	part_count = 0;
	while (cur_part) {
		part_count++;
		cur_part = cur_part->next;
	}

	/* Determine the sectors needed for MBR, GPT header and partition
	 * entries */
	cur_sect = 2; /* MBR + GPT header */
	header_length = part_count * PART_ENTRY_SIZE;
	header_sectors = header_length / sect_size;
	if (header_length % sect_size) {
		header_sectors++;
	}

	/* The GPT entry array must be a minimum of 16,384 bytes (reports
	 * wikipedia and testdisk, but not the UEFI spec)
	 */
	if (header_sectors < (int)(16384 / sect_size)) {
		header_sectors = (int)(16384 / sect_size);
	}

	cur_sect += header_sectors;
	first_usable_sector = cur_sect;

	cur_part = first_part;
	while (cur_part) {
		long cur_part_file_len;

		cur_part_id++;

		if (guid_is_zero(&cur_part->type)) {
			fprintf(stderr,
				"partition type not specified for partition "
				"%i\n",
				cur_part_id);
			return -1;
		}

		/* TODO is this appropriate? check the spec! */
		if (guid_is_zero(&cur_part->uuid)) {
			random_guid(&cur_part->uuid);
		}

		if (cur_part->sect_start == 0) {
			cur_part->sect_start = cur_sect;
		} else if (cur_part->sect_start < cur_sect) {
			fprintf(stderr,
				"unable to start partition %i at sector %i "
				"(would conflict with other data)\n",
				cur_part_id, cur_part->sect_start);
			return -1;
		}

		fseek(cur_part->src, 0, SEEK_END);
		cur_part_file_len = ftell(cur_part->src);
		fseek(cur_part->src, 0, SEEK_SET);

		if (cur_part->sect_length == 0) {
			cur_part->sect_length = cur_part_file_len / sect_size;
			if (cur_part_file_len % sect_size) {
				cur_part->sect_length++;
			}
		}
		cur_sect = cur_part->sect_start + cur_part->sect_length;

		cur_part = cur_part->next;
	}

	/* Add space for the secondary GPT */
	needed_file_length = cur_sect + 1 + header_sectors;

	if (image_sects == 0) {
		if (needed_file_length > min_image_sects) {
			image_sects = needed_file_length;
		} else {
			image_sects = min_image_sects;
		}
	} else if (image_sects < needed_file_length) {
		fprintf(stderr,
			"requested image size (%zu) is too small to hold the "
			"partitions\n",
			image_sects * sect_size);
		return -1;
	}

	secondary_headers_sect = image_sects - 1 - header_sectors;
	secondary_gpt_sect = image_sects - 1;

	return 0;
}

static void
panic(const char *msg)
{
	fprintf(stderr, "panic: %s", msg);
	exit(EXIT_FAILURE);
}

/*
 * Write the "protective MBR" to FILE *output.
 */
static void
write_mbr(void)
{
	uint8_t mbr[MAX_SECTOR_SIZE] = {0};
	assert(sect_size <= sizeof(mbr));

	/* entry for "Partition 1" starts here */
	uint8_t *p1 = mbr + 446;

	/* boot indicator = 0, start CHS = 0x000200 */
	set_u32(p1 + 0, 0x00020000);
	/* OSType 0xee = GPT Protective, EndingCHS = 0xffffff */
	set_u32(p1 + 4, 0xffffffee);
	/* StartingLBA = 1 */
	set_u32(p1 + 8, 0x00000001);
	/* number of sectors in partition */
	if (image_sects > 0xffffffff) {
		set_u32(p1 + 12, 0xffffffff);
	} else {
		assert(image_sects > 1); /* 0 sectors is not allowed */
		set_u32(p1 + 12, image_sects - 1);
	}
	/* Signature */
	set_u16(mbr + 510, 0xaa55);

	if (fwrite(mbr, 1, sect_size, output) != sect_size) {
		panic("fwrite failed");
	}
}

static void
write_output(void)
{
	uint8_t *parts;
	struct partition *cur_part;

	write_mbr();

	/* Define GPT headers */
	uint8_t gpt[MAX_SECTOR_SIZE] = {0};
	assert(sect_size <= sizeof(gpt));
	uint8_t gpt2[MAX_SECTOR_SIZE] = {0};
	assert(sect_size <= sizeof(gpt2));

	set_u64(gpt + 0, 0x5452415020494645ULL); /* Signature */
	set_u32(gpt + 8, 0x00010000UL); /* Revision */
	set_u32(gpt + 12, GPT_HEADER_SIZE); /* HeaderSize */
	set_u32(gpt + 16, 0); /* HeaderCRC32 */
	set_u32(gpt + 20, 0); /* Reserved */
	set_u64(gpt + 24, 0x1); /* MyLBA */
	set_u64(gpt + 32, secondary_gpt_sect); /* AlternateLBA */
	set_u64(gpt + 40, first_usable_sector); /* FirstUsableLBA */
	set_u64(gpt + 48, secondary_headers_sect - 1); /* LastUsableLBA */
	guid_to_bytestring(gpt + 56, &disk_guid); /* DiskGUID */
	set_u64(gpt + 72, 0x2); /* PartitionEntryLBA */
	set_u32(gpt + 80, part_count); /* NumberOfPartitionEntries */
	set_u32(gpt + 84, PART_ENTRY_SIZE); /* SizeOfPartitionEntry */
	set_u32(gpt + 88, 0); /* PartitionEntryArrayCRC32 */

	/* Define GPT partition entries */
	parts = calloc(header_sectors, sect_size);
	if (parts == NULL) {
		panic("calloc failed");
	}

	cur_part = first_part;
	int i = 0;
	while (cur_part) {
		int char_id;

		guid_to_bytestring(parts + i * PART_ENTRY_SIZE + 0,
			&cur_part->type); /* PartitionTypeGUID */
		guid_to_bytestring(parts + i * PART_ENTRY_SIZE + 16,
			&cur_part->uuid); /* UniquePartitionGUID */
		set_u64(parts + i * PART_ENTRY_SIZE + 32,
			cur_part->sect_start); /* StartingLBA */
		set_u64(parts + i * PART_ENTRY_SIZE + 40,
			cur_part->sect_start + cur_part->sect_length -
				1); /* EndingLBA */
		set_u64(parts + i * PART_ENTRY_SIZE + 48,
			cur_part->attrs); /* Attributes */

		/*
		 * TODO settle missing UTF-16LE conversion issue somehow,
		 * possibly by simply limiting the tool to ASCII here?
		 * TODO used to be "&& char_id < 35" but MAX_PART_NAME is 36
		 * now so which is correct? do we need a "double zero" at the
		 * end or not? what does the spec say? sfdisk works with 36
		 * chars and if we try 37 it just ignores the extra one, so
		 * there's some indication that no "double zero" is needed and
		 * that 36 is indeed the correct limit
		 */
		int len = min(strlen(cur_part->name), MAX_PART_NAME);
		for (char_id = 0; char_id < len; char_id++) {
			set_u16(parts + i * PART_ENTRY_SIZE + 56 + char_id * 2,
				cur_part->name[char_id]);
		}

		i++;
		cur_part = cur_part->next;
	}

	/* Do CRC calculations on the partition table entries and GPT headers */
	uint32_t parts_crc;
	CalculateCrc32(parts, part_count * PART_ENTRY_SIZE, &parts_crc);
	set_u32(gpt + 88, parts_crc);

	uint32_t gpt_crc;
	CalculateCrc32(gpt, GPT_HEADER_SIZE, &gpt_crc);
	set_u32(gpt + 16, gpt_crc);

	memcpy(gpt2, gpt, GPT_HEADER_SIZE);
	set_u32(gpt2 + 16, 0); /* HeaderCRC32 */
	set_u64(gpt2 + 24, secondary_gpt_sect); /* MyLBA */
	set_u64(gpt2 + 32, 0x1); /* AlternateLBA */
	set_u64(gpt2 + 72, secondary_headers_sect); /* PartitionEntryLBA */

	uint32_t gpt2_crc;
	CalculateCrc32(gpt2, GPT_HEADER_SIZE, &gpt2_crc);
	set_u32(gpt2 + 16, gpt2_crc);

	/* Write primary GPT and headers */
	if (fwrite(gpt, 1, sect_size, output) != sect_size) {
		panic("fwrite failed");
	}
	if (fwrite(parts, 1, header_sectors * sect_size, output) !=
		header_sectors * sect_size) {
		panic("fwrite failed");
	}

	/* Write partitions */
	cur_part = first_part;
	uint8_t image_buf[MAX_SECTOR_SIZE] = {0};
	assert(sect_size <= sizeof(image_buf));
	while (cur_part) {
		size_t bytes_read;
		size_t bytes_written = 0;

		fseek(output, cur_part->sect_start * sect_size, SEEK_SET);
		while ((bytes_read = fread(
				image_buf, 1, sect_size, cur_part->src)) > 0) {
			size_t bytes_to_write = bytes_read;

			/* Determine how much to write */
			if ((bytes_written + bytes_to_write) >
				(size_t)(cur_part->sect_length * sect_size))
				bytes_to_write =
					cur_part->sect_length * sect_size -
					bytes_written;

			if (fwrite(image_buf, 1, bytes_to_write, output) !=
				bytes_to_write) {
				panic("fwrite failed");
			}

			bytes_written += bytes_to_write;
		}

		cur_part = cur_part->next;
	}

	/* Write secondary GPT partition headers and header */
	fseek(output, secondary_headers_sect * sect_size, SEEK_SET);
	if (fwrite(parts, 1, header_sectors * sect_size, output) !=
		header_sectors * sect_size) {
		panic("fwrite failed");
	}
	fseek(output, secondary_gpt_sect * sect_size, SEEK_SET);
	if (fwrite(gpt2, 1, sect_size, output) != sect_size) {
		panic("fwrite failed");
	}

	free(parts);
}
