/**
 * @file hal_uart_stm32.cpp
 * @brief STM32/ChibiOS implementation of the portable UART HAL.
 */

#include "global.h"

#include "hal/hal_uart.h"

#if EFI_PROD_CODE || defined(__DOXYGEN__)

void hal_uart_sd_start(void* serial_driver, const void* raw_serial_config) {
	if (serial_driver == nullptr || raw_serial_config == nullptr) {
		return;
	}

	sdStart(reinterpret_cast<SerialDriver*>(serial_driver), reinterpret_cast<const SerialConfig*>(raw_serial_config));
}

void hal_uart_sd_stop(void* serial_driver) {
	if (serial_driver == nullptr) {
		return;
	}

	sdStop(reinterpret_cast<SerialDriver*>(serial_driver));
}

#else /* EFI_PROD_CODE */

void hal_uart_sd_start(void*, const void*) {
}

void hal_uart_sd_stop(void*) {
}

#endif /* EFI_PROD_CODE */
