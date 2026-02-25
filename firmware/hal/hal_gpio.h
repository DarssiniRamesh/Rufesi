#ifndef RUSEFI_HAL_GPIO_H
#define RUSEFI_HAL_GPIO_H

/**
 * @file hal_gpio.h
 * @brief Portable HAL interface for basic GPIO operations (read/write/mode).
 *
 * This header intentionally avoids exposing vendor/RTOS-specific GPIO types.
 * The underlying implementation is provided by a platform layer (e.g. STM32/ChibiOS).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque GPIO port handle. On STM32/ChibiOS this is typically a GPIO port pointer.
 */
typedef void* hal_gpio_port_t;

/**
 * GPIO pin index within a port.
 */
typedef uint16_t hal_gpio_pin_t;

/**
 * PLATFORM NOTE:
 *  - "raw mode" is platform-specific (e.g. ChibiOS iomode_t with PAL_MODE_* macros).
 *  - Use raw mode when existing code already uses PAL_MODE_* macros and you want to
 *    keep those details out of application logic.
 */

/**
 * PUBLIC_INTERFACE
 * @brief Write a digital value to a GPIO pad.
 * @param port Opaque port handle.
 * @param pin Pin index within the port.
 * @param value 0=low, non-zero=high.
 */
void hal_gpio_write(hal_gpio_port_t port, hal_gpio_pin_t pin, int value);

/**
 * PUBLIC_INTERFACE
 * @brief Read a digital value from a GPIO pad.
 * @param port Opaque port handle.
 * @param pin Pin index within the port.
 * @return 0 or 1.
 */
int hal_gpio_read(hal_gpio_port_t port, hal_gpio_pin_t pin);

/**
 * PUBLIC_INTERFACE
 * @brief Configure a GPIO pad using a platform-specific raw mode value.
 * @param port Opaque port handle.
 * @param pin Pin index within the port.
 * @param raw_mode Platform-specific mode encoding (ex: ChibiOS PAL_MODE_*).
 */
void hal_gpio_set_mode_raw(hal_gpio_port_t port, hal_gpio_pin_t pin, uint32_t raw_mode);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HAL_GPIO_H */
