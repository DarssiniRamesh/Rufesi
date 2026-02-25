#ifndef RUSEFI_HAL_CAN_H
#define RUSEFI_HAL_CAN_H

/**
 * @file hal_can.h
 * @brief Portable HAL interface for CAN driver lifecycle and TX/RX.
 *
 * This interface uses opaque handles and "raw config" pointers to avoid coupling
 * higher-level logic to STM32/ChibiOS symbols (CAND1, canStart, etc).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* hal_can_driver_t;

typedef enum {
	HAL_CAN_DRIVER_1 = 1,
	HAL_CAN_DRIVER_2 = 2,
} hal_can_driver_index_t;

/**
 * PUBLIC_INTERFACE
 * @brief Obtain a default CAN driver handle by index (e.g. CAN1/CAN2).
 */
hal_can_driver_t hal_can_get_driver(hal_can_driver_index_t index);

/**
 * PUBLIC_INTERFACE
 * @brief Start a CAN driver.
 * @param driver Driver handle from hal_can_get_driver()
 * @param raw_config Platform-specific config pointer (e.g. ChibiOS CANConfig*)
 */
void hal_can_start(hal_can_driver_t driver, const void* raw_config);

/**
 * PUBLIC_INTERFACE
 * @brief Transmit a CAN frame.
 * @param driver Driver handle.
 * @param mailbox Mailbox selector (platform-specific, typically CAN_ANY_MAILBOX).
 * @param raw_frame Platform-specific frame pointer (e.g. ChibiOS CANTxFrame*).
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, non-zero on failure.
 */
int hal_can_transmit(hal_can_driver_t driver, int mailbox, const void* raw_frame, uint32_t timeout_ms);

/**
 * PUBLIC_INTERFACE
 * @brief Receive a CAN frame.
 * @param driver Driver handle.
 * @param mailbox Mailbox selector (platform-specific, typically CAN_ANY_MAILBOX).
 * @param raw_frame_out Platform-specific frame pointer to fill (e.g. ChibiOS CANRxFrame*).
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, non-zero on timeout/failure.
 */
int hal_can_receive(hal_can_driver_t driver, int mailbox, void* raw_frame_out, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HAL_CAN_H */
