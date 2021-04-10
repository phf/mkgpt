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

#include "guid.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GUID_FMT                                                               \
	"%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX"

int
guid_to_string(char *str, const GUID *guid)
{
	if (guid == NULL) {
		return -1;
	}
	if (str == NULL) {
		return -1;
	}

	sprintf(str, GUID_FMT, guid->data1, guid->data2, guid->data3,
		guid->data4[0], guid->data4[1], guid->data4[2], guid->data4[3],
		guid->data4[4], guid->data4[5], guid->data4[6], guid->data4[7]);

	return 0;
}

int
string_to_guid(GUID *guid, const char *str)
{
	if (guid == NULL) {
		return -1;
	}
	if (str == NULL) {
		return -1;
	}

	sscanf(str, GUID_FMT, &guid->data1, &guid->data2, &guid->data3,
		&guid->data4[0], &guid->data4[1], &guid->data4[2],
		&guid->data4[3], &guid->data4[4], &guid->data4[5],
		&guid->data4[6], &guid->data4[7]);

	return 0;
}

int
guid_to_bytestring(uint8_t *bytes, const GUID *guid)
{
	if (guid == NULL) {
		return -1;
	}
	if (bytes == NULL) {
		return -1;
	}

	/* TODO potential alignment problem? */
	*(uint32_t *)&bytes[0] = guid->data1;
	*(uint16_t *)&bytes[4] = guid->data2;
	*(uint16_t *)&bytes[6] = guid->data3;
	for (int i = 0; i < 8; i++) {
		bytes[8 + i] = guid->data4[i];
	}

	return 0;
}

int
guid_is_zero(const GUID *guid)
{
	if (guid->data1 != 0) {
		return 0;
	}
	if (guid->data2 != 0) {
		return 0;
	}
	if (guid->data3 != 0) {
		return 0;
	}
	for (int i = 0; i < 8; i++) {
		if (guid->data4[i] != 0) {
			return 0;
		}
	}
	return 1;
}

static uint32_t
rnd()
{
	return random() & 0xff; /* just a byte please */
}

int
random_guid(GUID *guid)
{
	if (guid == NULL) {
		return -1;
	}

	static int initialized = 0;
	if (!initialized) {
		srandom(time(NULL));
		initialized = 1;
	}

	guid->data1 = rnd() | rnd() << 8 | rnd() << 16 | rnd() << 24;
	guid->data2 = rnd() | rnd() << 8;
	guid->data3 = rnd() | ((rnd() & 0x0f) | 0x40) << 8;
	for (int i = 0; i < 8; i++) {
		guid->data4[i] = rnd();
	}

	return 0;
}
