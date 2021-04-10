#pragma once

#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

int
CalculateCrc32(uint8_t *Data, size_t DataSize, uint32_t *CrcOut);

#endif
