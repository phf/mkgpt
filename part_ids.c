#include "part_ids.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define MBR_SWAP 0x82
#define MBR_LINUX 0x83
#define EFI_SYSTEM 0x100
#define BIOS_BOOT 0x101

#define GUID_TABLE \
	X(SWAP, "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F") \
	X(LINUX, "0FC63DAF-8483-4772-8E79-3D69D8477DE4") \
	X(SYSTEM, "C12A7328-F81F-11D2-BA4B-00A0C93EC93B") \
	X(BDP, "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7") \
	X(BIOS, "21686148-6449-6E6F-744E-656564454649")

#define ALIAS_TABLE \
	X("fat12", 0x01, BDP) \
	X("fat16", 0x04, BDP) \
	X("fat16b", 0x06, BDP) \
	X("ntfs", 0x07, BDP) \
	X("fat32", 0x0b, BDP) \
	X("fat32x", 0x0c, BDP) \
	X("fat16x", 0x0e, BDP) \
	X("fat16+", 0x28, BDP) \
	X("fat32+", 0x29, BDP) \
	X("swap", MBR_SWAP, SWAP) \
	X("linux", MBR_LINUX, LINUX) \
	X("system", EFI_SYSTEM, SYSTEM) \
	X("bios", BIOS_BOOT, BIOS)

/* First we generate compact indices for each GUID using an enum. */
enum GUID_INDEX {
#define X(index, guid) GUID_ ## index,
	GUID_TABLE
#undef X
	NUM_GUIDS,
};

/*
 * Then we plop down the GUIDs in a compact table. Overly compact maybe? Note
 * that there are no pointers involved and no NUL terminators at the "end" of
 * a GUID, so don't try to `printf` them directly!
 */
static char guids[NUM_GUIDS][GUID_STRLEN] = {
#define X(index, guid) [GUID_ ## index] = guid,
	GUID_TABLE
#undef X
};

/*
 * Now for the aliases that "point" to the GUIDs. Note that these are *not*
 * sorted, but unless we have thousands of names, a linear search should be
 * all we need. A `key` of `NULL` marks the end.
 */
static struct {char *key; int value;} name_to_guid[] = {
#define X(name, id, index) {name, GUID_ ## index},
	ALIAS_TABLE
#undef X
	{NULL, -1},
};

/*
 * The same for the "short" MBR-style ids. A `key` of `-1` marks the end.
 */
static struct {int key; int value;} mbr_to_guid[] = {
#define X(name, id, index) {id, GUID_ ## index},
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
	for (int i = 0; name_to_guid[i].key != NULL; i++) {
		if (!strcmp(str, name_to_guid[i].key)) {
			return string_to_guid(guid, guids[name_to_guid[i].value]);
		}
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
	for (int i = 0; mbr_to_guid[i].key != -1; i++) {
		if (mbr_to_guid[i].key == mbr_id) {
			return string_to_guid(guid, guids[mbr_to_guid[i].value]);
		}
	}

	return -1;
}
