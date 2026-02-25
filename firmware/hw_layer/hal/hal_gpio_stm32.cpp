/**
 * @file hal_gpio_stm32.cpp
 * @brief STM32/ChibiOS implementation of the portable GPIO HAL.
 */

#include "global.h"

#include "hal/hal_gpio.h"

#if EFI_PROD_CODE || defined(__DOXYGEN__)

void hal_gpio_write(hal_gpio_port_t port, hal_gpio_pin_t pin, int value) {
	if (port == nullptr) {
		return;
	}

	// ChibiOS PAL expects ioportid_t (GPIO port pointer) and pad index.
	palWritePad(reinterpret_cast<ioportid_t>(port), static_cast<uint8_t>(pin), value ? PAL_HIGH : PAL_LOW);
}

int hal_gpio_read(hal_gpio_port_t port, hal_gpio_pin_t pin) {
	if (port == nullptr) {
		return 0;
	}

	return palReadPad(reinterpret_cast<ioportid_t>(port), static_cast<uint8_t>(pin)) ? 1 : 0;
}

void hal_gpio_set_mode_raw(hal_gpio_port_t port, hal_gpio_pin_t pin, uint32_t raw_mode) {
	if (port == nullptr) {
		return;
	}

	palSetPadMode(reinterpret_cast<ioportid_t>(port), static_cast<uint8_t>(pin), static_cast<iomode_t>(raw_mode));
}

#else /* EFI_PROD_CODE */

// Simulator/non-prod: provide safe stubs.

void hal_gpio_write(hal_gpio_port_t, hal_gpio_pin_t, int) {
}

int hal_gpio_read(hal_gpio_port_t, hal_gpio_pin_t) {
	return 0;
}

void hal_gpio_set_mode_raw(hal_gpio_port_t, hal_gpio_pin_t, uint32_t) {
}

#endif /* EFI_PROD_CODE */
