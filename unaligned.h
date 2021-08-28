#pragma once

/* SPDX-License-Identifier: MIT */

#ifndef UNALIGNED_H
#define UNALIGNED_H

/*
 * Unaligned little-endian memory access. See Chris Wellons' excellent post at
 * https://nullprogram.com/blog/2016/11/22/ if you're confused by this.
 */

#include <stdint.h>

static inline uint16_t
get_u16(const uint8_t *buf)
{
	return (uint16_t)buf[1] << 8 | (uint16_t)buf[0] << 0;
}

static inline uint32_t
get_u32(const uint8_t *buf)
{
	return (uint32_t)buf[3] << 24 | (uint32_t)buf[2] << 16 |
	       (uint32_t)buf[1] << 8 | (uint32_t)buf[0] << 0;
}

static inline uint64_t
get_u64(const uint8_t *buf)
{
	return (uint64_t)buf[7] << 56 | (uint64_t)buf[6] << 48 |
	       (uint64_t)buf[5] << 40 | (uint64_t)buf[4] << 32 |
	       (uint64_t)buf[3] << 24 | (uint64_t)buf[2] << 16 |
	       (uint64_t)buf[1] << 8 | (uint64_t)buf[0] << 0;
}

static inline void
set_u16(uint8_t *buf, const uint16_t val)
{
	buf[0] = val >> 0;
	buf[1] = val >> 8;
}

static inline void
set_u32(uint8_t *buf, const uint32_t val)
{
	buf[0] = val >> 0;
	buf[1] = val >> 8;
	buf[2] = val >> 16;
	buf[3] = val >> 24;
}

static inline void
set_u64(uint8_t *buf, const uint64_t val)
{
	buf[0] = val >> 0;
	buf[1] = val >> 8;
	buf[2] = val >> 16;
	buf[3] = val >> 24;
	buf[4] = val >> 32;
	buf[5] = val >> 40;
	buf[6] = val >> 48;
	buf[7] = val >> 56;
}

#endif
