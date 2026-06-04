// SPDX-FileCopyrightText: 2026 ANSSI
// SPDX-License-Identifier: Apache-2.0

#include <bsp/drivers/dma/gpdma.h>

#include <sentry/ktypes.h>

kstatus_t gpdma_probe(uint8_t controller)
{
    (void)controller;
    return K_ERROR_NOENT;
}

kstatus_t gpdma_channel_clear_status(gpdma_stream_cfg_t const * const desc)
{
    (void)desc;
    return K_ERROR_NOENT;
}

kstatus_t gpdma_channel_get_status(gpdma_stream_cfg_t const * const desc, gpdma_chan_status_t *status)
{
    (void)desc;
    if (unlikely(status == NULL)) {
        return K_ERROR_INVPARAM;
    }
    *status = GPDMA_CHAN_STATE_UNSET;
    return K_ERROR_NOENT;
}

kstatus_t gpdma_channel_configure(gpdma_stream_cfg_t const * const desc)
{
    (void)desc;
    return K_ERROR_NOENT;
}

kstatus_t gpdma_channel_enable(gpdma_stream_cfg_t const * const desc)
{
    (void)desc;
    return K_ERROR_NOENT;
}

kstatus_t gpdma_get_interrupt(gpdma_stream_cfg_t const * const desc, uint16_t * const IRQn)
{
    (void)desc;
    if (unlikely(IRQn == NULL)) {
        return K_ERROR_INVPARAM;
    }
    *IRQn = 0;
    return K_ERROR_NOENT;
}

bool gpdma_irq_is_dma_owned(uint16_t IRQn)
{
    (void)IRQn;
    return false;
}

kstatus_t gpdma_interrupt_clear(gpdma_stream_cfg_t const * const desc)
{
    (void)desc;
    return K_ERROR_NOENT;
}

kstatus_t gpdma_channel_suspend(gpdma_stream_cfg_t const * const desc)
{
    (void)desc;
    return K_ERROR_NOENT;
}

kstatus_t gpdma_channel_resume(gpdma_stream_cfg_t const * const desc)
{
    (void)desc;
    return K_ERROR_NOENT;
}

kstatus_t gpdma_channel_reset(gpdma_stream_cfg_t const * const desc)
{
    (void)desc;
    return K_ERROR_NOENT;
}
