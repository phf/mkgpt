/* SPDX-License-Identifier: MIT */

#include "part_ids.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define GUID_TABLE                                                             \
	X(LINUX_SWAP, "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F")                  \
	X(LINUX_FS, "0FC63DAF-8483-4772-8E79-3D69D8477DE4")                    \
	X(EFI_SYSTEM, "C12A7328-F81F-11D2-BA4B-00A0C93EC93B")                  \
	X(MS_BASIC_DATA, "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7")               \
	X(BIOS_BOOT, "21686148-6449-6E6F-744E-656564454649")

#define ALIAS_TABLE                                                            \
	X("fat12", 0x01, MS_BASIC_DATA)                                        \
	X("fat16", 0x04, MS_BASIC_DATA)                                        \
	X("fat16b", 0x06, MS_BASIC_DATA)                                       \
	X("ntfs", 0x07, MS_BASIC_DATA)                                         \
	X("fat32", 0x0b, MS_BASIC_DATA)                                        \
	X("fat32x", 0x0c, MS_BASIC_DATA)                                       \
	X("fat16x", 0x0e, MS_BASIC_DATA)                                       \
	X("fat16+", 0x28, MS_BASIC_DATA)                                       \
	X("fat32+", 0x29, MS_BASIC_DATA)                                       \
	X("swap", 0x82, LINUX_SWAP)                                            \
	X("linux", 0x83, LINUX_FS)                                             \
	X("system", 0x100, EFI_SYSTEM)                                         \
	X("bios", 0x101, BIOS_BOOT)

/*
 * First we generate compact indices for each GUID using an enum. (It's true,
 * clang-format is not improving things below.)
 */
enum GUID_INDEX {
#define X(index, guid) GUID_##index,
	GUID_TABLE
#undef X
		NUM_GUIDS
};

/*
 * Then we plop down the GUIDs in a compact table. Overly compact maybe? Note
 * that there are no pointers involved and no NUL terminators at the "end" of
 * a GUID, so don't try to `printf` them directly!
 */
static const char guids[NUM_GUIDS][GUID_STRLEN] = {
#define X(index, guid) [GUID_##index] = guid,
	GUID_TABLE
#undef X
};

/*
 * Now for the aliases that "point" to the GUIDs. Note that these are *not*
 * sorted, but unless we have thousands of names, a linear search should be
 * all we need. A `key` of `NULL` marks the end.
 */
static const struct {
	char *key;
	int value;
} name_to_guid[] = {
#define X(name, id, index) {name, GUID_##index},
	ALIAS_TABLE
#undef X
	{NULL, -1},
};

/*
 * The same for the "short" MBR-style ids. A `key` of `-1` marks the end.
 */
static const struct {
	int key;
	int value;
} mbr_to_guid[] = {
#define X(name, id, index) {id, GUID_##index},
	ALIAS_TABLE
#undef X
	{-1, -1},
};

int
valid_string_guid(const char str[GUID_STRLEN])
{
	/* TODO there may be a simpler way? but let's avoid regexp stuff... */

	static const int spans[5] = {8, 4, 4, 4, 12}; /* spans of hex digits */
	/* (sum spans) + 4 == GUID_STRLEN */

	int pos = 0;

	/* for all spans */
	for (int i = 0; i < 5; i++) {
		/* for all hex digits in span */
		for (int j = 0; j < spans[i]; j++) {
			if (!isxdigit(str[pos])) {
				return 0;
			}
			pos++;
		}
		/* need a '-' except after the last span */
		if (i == 4) {
			break;
		}

		if (str[pos] != '-') {
			return 0;
		}
		pos++;
	}

	return 1;
}

static int
find_by_name(const char *str, GUID *guid)
{
	for (int i = 0; name_to_guid[i].key != NULL; i++) {
		if (!strcmp(str, name_to_guid[i].key)) {
			return string_to_guid(
				guid, guids[name_to_guid[i].value]);
		}
	}
	return -1;
}

static int
find_by_id(const int id, GUID *guid)
{
	for (int i = 0; mbr_to_guid[i].key != -1; i++) {
		if (mbr_to_guid[i].key == id) {
			return string_to_guid(
				guid, guids[mbr_to_guid[i].value]);
		}
	}
	return -1;
}

/*
 * Parse GUID/UUID notation used on the command line.
 * Returns 0 on success.
 */
int
parse_guid(const char *str, GUID *guid)
{
	/* detect request for random uuid */
	if (!strcmp(str, "random") || !strcmp(str, "rnd")) {
		return random_guid(guid);
	}

	/* detect by name */
	if (find_by_name(str, guid) == 0) {
		return 0;
	}

	/* try and parse as guid */
	if (valid_string_guid(str)) {
		return string_to_guid(guid, str);
	}

	/* detect mbr partition id by number */
	errno = 0;
	long mbr_id = strtol(str, NULL, 0);
	if (errno != 0) {
		return -1;
	}
	if (find_by_id(mbr_id, guid) == 0) {
		return 0;
	}

	return -1;
}
