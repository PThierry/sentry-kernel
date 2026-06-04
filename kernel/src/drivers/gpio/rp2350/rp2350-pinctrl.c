// SPDX-FileCopyrightText: 2026 ANSSI
// SPDX-License-Identifier: Apache-2.0

#include <bsp/drivers/gpio/pinctrl.h>

#include <sentry/ktypes.h>

kstatus_t gpio_pinctrl_configure(gpio_pinctrl_desc_t pinctrl_desc)
{
    (void)pinctrl_desc;
    return K_STATUS_OKAY;
}
