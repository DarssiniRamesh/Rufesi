#ifndef RUSEFI_CAN_CORE_H
#define RUSEFI_CAN_CORE_H

/**
 * @file rusefi_can_core.h
 * @brief RTOS-agnostic, portable CAN core for headless module.
 *
 * This file provides:
 *  - A portable CAN frame representation (no ChibiOS/RTOS types)
 *  - A small handler registry with ID/mask matching
 *  - RX dispatch
 *  - A minimal TX API surface via an injected interface (function pointer)
 *
 * The core does not perform any IO by itself. IO is performed by user-provided
 * callbacks (adapter layer).
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Flags for rusefi_can_frame_t::flags.
 *
 * Note: These are kept minimal for portability. More flags can be added later
 * without affecting existing behavior.
 */
typedef enum {
	RUSEFI_CAN_FRAME_FLAG_EXT = (1u << 0), /**< Extended ID (29-bit) */
	RUSEFI_CAN_FRAME_FLAG_RTR = (1u << 1), /**< Remote transmission request */
} rusefi_can_frame_flags_t;

/**
 * @brief Portable CAN frame representation.
 */
typedef struct {
	uint32_t id;      /**< Standard or extended CAN ID (interpret via flags) */
	uint8_t dlc;      /**< Payload length [0..8] */
	uint8_t data[8];  /**< Payload bytes */
	uint8_t flags;    /**< rusefi_can_frame_flags_t */
} rusefi_can_frame_t;

typedef struct rusefi_can_core rusefi_can_core_t;

/**
 * @brief RX handler signature.
 *
 * @param user_ctx User-supplied pointer at registration time.
 * @param frame Received frame.
 */
typedef void (*rusefi_can_rx_handler_t)(void* user_ctx, const rusefi_can_frame_t* frame);

/**
 * @brief Minimal TX interface signature.
 *
 * Implementations should send the frame to hardware/transport.
 *
 * @return 0 on success, non-zero on failure.
 */
typedef int (*rusefi_can_tx_fn_t)(void* user_ctx, const rusefi_can_frame_t* frame);

/**
 * @brief TX interface (function pointer + opaque user context).
 */
typedef struct {
	rusefi_can_tx_fn_t send;
	void* user_ctx;
} rusefi_can_tx_iface_t;

/**
 * @brief A single RX handler registration entry.
 *
 * Matching is performed using:
 *   (frame->id & id_mask) == (id_filter & id_mask)
 * Optionally, required_flags can be used to match on EXT/RTR bits.
 */
typedef struct {
	uint32_t id_filter;
	uint32_t id_mask;
	uint8_t required_flags;
	rusefi_can_rx_handler_t handler;
	void* user_ctx;
} rusefi_can_rx_registration_t;

/**
 * PUBLIC_INTERFACE
 * @brief Initialize a CAN core instance.
 *
 * @param core Pointer to core storage.
 */
void rusefi_can_core_init(rusefi_can_core_t* core);

/**
 * PUBLIC_INTERFACE
 * @brief Deinitialize a CAN core instance.
 *
 * Currently a no-op; provided for symmetry and future expansion.
 */
void rusefi_can_core_deinit(rusefi_can_core_t* core);

/**
 * PUBLIC_INTERFACE
 * @brief Configure the TX interface used by rusefi_can_core_send().
 *
 * Passing NULL disables TX.
 */
void rusefi_can_core_set_tx_iface(rusefi_can_core_t* core, const rusefi_can_tx_iface_t* tx_iface);

/**
 * PUBLIC_INTERFACE
 * @brief Register an RX handler.
 *
 * @param core Core instance.
 * @param reg Registration parameters.
 * @return 0 on success, non-zero on failure (e.g., registry full/invalid args).
 */
int rusefi_can_core_register_rx_handler(rusefi_can_core_t* core, const rusefi_can_rx_registration_t* reg);

/**
 * PUBLIC_INTERFACE
 * @brief Dispatch a received frame to matching handlers.
 *
 * @param core Core instance.
 * @param frame Received frame.
 * @return Number of handlers invoked.
 */
int rusefi_can_core_dispatch_rx(rusefi_can_core_t* core, const rusefi_can_frame_t* frame);

/**
 * PUBLIC_INTERFACE
 * @brief Send a frame using configured TX interface.
 *
 * @return 0 on success, non-zero on failure (e.g., TX not configured).
 */
int rusefi_can_core_send(rusefi_can_core_t* core, const rusefi_can_frame_t* frame);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_CAN_CORE_H */
