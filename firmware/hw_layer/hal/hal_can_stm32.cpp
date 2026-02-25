/**
 * @file hal_can_stm32.cpp
 * @brief STM32/ChibiOS implementation of the portable CAN HAL.
 */

#include "global.h"

#include "hal/hal_can.h"

#if EFI_CAN_SUPPORT || defined(__DOXYGEN__)

hal_can_driver_t hal_can_get_driver(hal_can_driver_index_t index) {
	switch (index) {
#if HAL_USE_CAN || defined(__DOXYGEN__)
		case HAL_CAN_DRIVER_1:
			return reinterpret_cast<hal_can_driver_t>(&CAND1);
#if STM32_CAN_USE_CAN2 || defined(__DOXYGEN__)
		case HAL_CAN_DRIVER_2:
			return reinterpret_cast<hal_can_driver_t>(&CAND2);
#endif /* STM32_CAN_USE_CAN2 */
#endif /* HAL_USE_CAN */
		default:
			return nullptr;
	}
}

void hal_can_start(hal_can_driver_t driver, const void* raw_config) {
	if (driver == nullptr || raw_config == nullptr) {
		return;
	}

	canStart(reinterpret_cast<CANDriver*>(driver), reinterpret_cast<const CANConfig*>(raw_config));
}

int hal_can_transmit(hal_can_driver_t driver, int mailbox, const void* raw_frame, uint32_t timeout_ms) {
	if (driver == nullptr || raw_frame == nullptr) {
		return -1;
	}

	msg_t result = canTransmit(
		reinterpret_cast<CANDriver*>(driver),
		mailbox,
		reinterpret_cast<const CANTxFrame*>(raw_frame),
		TIME_MS2I(timeout_ms)
	);

	return (result == MSG_OK) ? 0 : -1;
}

int hal_can_receive(hal_can_driver_t driver, int mailbox, void* raw_frame_out, uint32_t timeout_ms) {
	if (driver == nullptr || raw_frame_out == nullptr) {
		return -1;
	}

	msg_t result = canReceive(
		reinterpret_cast<CANDriver*>(driver),
		mailbox,
		reinterpret_cast<CANRxFrame*>(raw_frame_out),
		TIME_MS2I(timeout_ms)
	);

	return (result == MSG_OK) ? 0 : -1;
}

#else /* EFI_CAN_SUPPORT */

hal_can_driver_t hal_can_get_driver(hal_can_driver_index_t) {
	return nullptr;
}

void hal_can_start(hal_can_driver_t, const void*) {
}

int hal_can_transmit(hal_can_driver_t, int, const void*, uint32_t) {
	return -1;
}

int hal_can_receive(hal_can_driver_t, int, void*, uint32_t) {
	return -1;
}

#endif /* EFI_CAN_SUPPORT */
