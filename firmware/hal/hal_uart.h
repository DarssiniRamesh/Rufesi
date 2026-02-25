#ifndef RUSEFI_HAL_UART_H
#define RUSEFI_HAL_UART_H

/**
 * @file hal_uart.h
 * @brief Portable HAL interface for UART/serial start/stop operations.
 *
 * For rusEFI this primarily wraps ChibiOS SerialDriver APIs in a way that
 * removes direct calls (sdStart/sdStop) from application logic.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PUBLIC_INTERFACE
 * @brief Start a ChibiOS SerialDriver-like device (sdStart wrapper).
 * @param serial_driver Opaque driver pointer (e.g. SerialDriver*).
 * @param raw_serial_config Platform-specific serial config (e.g. SerialConfig*).
 */
void hal_uart_sd_start(void* serial_driver, const void* raw_serial_config);

/**
 * PUBLIC_INTERFACE
 * @brief Stop a ChibiOS SerialDriver-like device (sdStop wrapper).
 * @param serial_driver Opaque driver pointer (e.g. SerialDriver*).
 */
void hal_uart_sd_stop(void* serial_driver);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HAL_UART_H */
