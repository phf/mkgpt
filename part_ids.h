#pragma once

/* SPDX-License-Identifier: MIT */

#ifndef PART_IDS_H
#define PART_IDS_H

#include "guid.h"

/* Length of string-format GUID *excluding* NUL terminator. */
#define GUID_STRLEN (sizeof("ABCDEF01-2345-6789-ABCD-EF0123456789") - 1)

int
valid_string_guid(const char str[GUID_STRLEN]);

int
parse_guid(const char *str, GUID *guid);

#endif
