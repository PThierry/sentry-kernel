// Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
// Copyright (c) 2025 H2Lab Development Team
// SPDX-License-Identifier: BSD-3-Clause

/**
 *
 * @file This file is part of the Raspberry Pi Pico SDK, and has been modified in order to
 * be included in the kernel.
 */

#include <inttypes.h>
#include "sysinfo.h"
#define SYSINFO_BASE 0x40000000UL
#define SYSINFO_CHIP_ID_REG 0x00UL

uint8_t sysinfo_chip_version(void)
{
    /* First register of sysinfo is chip id */
    uint32_t chip_id = *(uint32_t*)(SYSINFO_BASE + SYSINFO_CHIP_ID_REG);
    uint32_t manufacturer = chip_id & SYSINFO_CHIP_ID_MANUFACTURER_BITS;
    uint32_t part = (chip_id & SYSINFO_CHIP_ID_PART_BITS) >> SYSINFO_CHIP_ID_PART_LSB;
    assert(manufacturer == MANUFACTURER_RPI);
    assert(part == PART_RP4);
    // 0 == A0, 1 == A1, 2 == A2
    uint version = (chip_id & SYSINFO_CHIP_ID_REVISION_BITS) >> SYSINFO_CHIP_ID_REVISION_LSB;
    return (uint8_t)version;
}
