// SPDX-FileCopyrightText: 2026 ANSSI
// SPDX-License-Identifier: Apache-2.0

#include <bsp/drivers/gpio/gpio.h>

#include <sentry/ktypes.h>

kstatus_t gpio_probe(uint8_t gpio_port_id)
{
    (void)gpio_port_id;
    return K_STATUS_OKAY;
}

kstatus_t gpio_set_mode(uint8_t gpio_port_id, uint8_t pin, gpio_mode_t mode)
{
    (void)gpio_port_id;
    (void)pin;
    (void)mode;
    return K_STATUS_OKAY;
}

kstatus_t gpio_set_pull_mode(uint8_t gpio_port_id, uint8_t pin, gpio_pullupd_t pupd)
{
    (void)gpio_port_id;
    (void)pin;
    (void)pupd;
    return K_STATUS_OKAY;
}

kstatus_t gpio_set_type(uint8_t gpio_port_id, uint8_t pin, gpio_type_t type)
{
    (void)gpio_port_id;
    (void)pin;
    (void)type;
    return K_STATUS_OKAY;
}

kstatus_t gpio_set_af(uint8_t gpio_port_id, uint8_t pin, gpio_af_t af)
{
    (void)gpio_port_id;
    (void)pin;
    (void)af;
    return K_STATUS_OKAY;
}

kstatus_t gpio_set_speed(uint8_t gpio_port_id, uint8_t pin, gpio_speed_t speed)
{
    (void)gpio_port_id;
    (void)pin;
    (void)speed;
    return K_STATUS_OKAY;
}

kstatus_t gpio_set(uint8_t gpio_port_id, uint8_t pin)
{
    (void)gpio_port_id;
    (void)pin;
    return K_STATUS_OKAY;
}

kstatus_t gpio_reset(uint8_t gpio_port_id, uint8_t pin)
{
    (void)gpio_port_id;
    (void)pin;
    return K_STATUS_OKAY;
}

kstatus_t gpio_get(uint8_t gpio_port_id, uint8_t pin, bool *val)
{
    (void)gpio_port_id;
    (void)pin;
    if (unlikely(val == NULL)) {
        return K_ERROR_INVPARAM;
    }
    *val = false;
    return K_STATUS_OKAY;
}
