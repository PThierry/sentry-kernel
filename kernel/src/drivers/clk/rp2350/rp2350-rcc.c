// SPDX-FileCopyrightText: 2026 ANSSI
// SPDX-License-Identifier: Apache-2.0

#include <bsp/drivers/clk/rcc.h>

#include <sentry/ktypes.h>

#define RP2350_CORE_CLK_HZ (150000000u)

kstatus_t rcc_probe(void)
{
    return K_STATUS_OKAY;
}

#if CONFIG_BUILD_TARGET_DEBUG
kstatus_t rcc_enable_debug_clockout(void)
{
    return K_STATUS_OKAY;
}
#endif

uint32_t rcc_get_core_frequency(void)
{
    return RP2350_CORE_CLK_HZ;
}

kstatus_t rcc_enable(bus_id_t busid, uint32_t clk_msk, rcc_opts_t flags)
{
    (void)busid;
    (void)clk_msk;
    (void)flags;
    return K_STATUS_OKAY;
}

kstatus_t rcc_disable(bus_id_t busid, uint32_t clk_msk, rcc_opts_t flags)
{
    (void)busid;
    (void)clk_msk;
    (void)flags;
    return K_STATUS_OKAY;
}

kstatus_t rcc_get_bus_clock(bus_id_t busid, uint32_t *busclk)
{
    (void)busid;
    if (unlikely(busclk == NULL)) {
        return K_ERROR_INVPARAM;
    }
    *busclk = RP2350_CORE_CLK_HZ;
    return K_STATUS_OKAY;
}

kstatus_t rcc_mux_select_clock_source(uint32_t clk_reg, uint32_t clkmsk, uint32_t val)
{
    (void)clk_reg;
    (void)clkmsk;
    (void)val;
    return K_STATUS_OKAY;
}
