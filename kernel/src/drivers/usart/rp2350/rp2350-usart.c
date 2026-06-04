// SPDX-FileCopyrightText: 2026 ANSSI
// SPDX-License-Identifier: Apache-2.0

#include <bsp/drivers/usart/usart.h>

#include <sentry/ktypes.h>

kstatus_t usart_probe(void)
{
    return K_STATUS_OKAY;
}

kstatus_t usart_tx(const uint8_t *data, size_t data_len)
{
    (void)data;
    (void)data_len;
    return K_STATUS_OKAY;
}
