#include "rusefi_can_core.h"

#include <string.h>

#ifndef RUSEFI_CAN_CORE_MAX_HANDLERS
/**
 * @brief Maximum number of RX handler registrations.
 *
 * Kept small and static for determinism and RTOS-agnostic behavior.
 */
#define RUSEFI_CAN_CORE_MAX_HANDLERS 16u
#endif

struct rusefi_can_core {
	rusefi_can_tx_iface_t tx;
	rusefi_can_rx_registration_t handlers[RUSEFI_CAN_CORE_MAX_HANDLERS];
	uint8_t handler_count;
};

static int is_valid_registration(const rusefi_can_rx_registration_t* reg) {
	if (reg == NULL) {
		return 0;
	}

	if (reg->handler == NULL) {
		return 0;
	}

	/* id_mask may be 0 (match-all), which is valid. */
	return 1;
}

static int frame_matches(const rusefi_can_rx_registration_t* reg, const rusefi_can_frame_t* frame) {
	if (((frame->id & reg->id_mask) != (reg->id_filter & reg->id_mask))) {
		return 0;
	}

	/* required_flags are "must be set" bits */
	if ((frame->flags & reg->required_flags) != reg->required_flags) {
		return 0;
	}

	return 1;
}

// PUBLIC_INTERFACE
void rusefi_can_core_init(rusefi_can_core_t* core) {
	/** Initialize the CAN core to a known empty state. */
	if (core == NULL) {
		return;
	}

	memset(core, 0, sizeof(*core));
}

// PUBLIC_INTERFACE
void rusefi_can_core_deinit(rusefi_can_core_t* core) {
	/** Deinitialize the CAN core (currently no-op). */
	(void)core;
}

// PUBLIC_INTERFACE
void rusefi_can_core_set_tx_iface(rusefi_can_core_t* core, const rusefi_can_tx_iface_t* tx_iface) {
	/** Configure (or clear) the TX callback interface. */
	if (core == NULL) {
		return;
	}

	if (tx_iface == NULL) {
		memset(&core->tx, 0, sizeof(core->tx));
		return;
	}

	core->tx = *tx_iface;
}

// PUBLIC_INTERFACE
int rusefi_can_core_register_rx_handler(rusefi_can_core_t* core, const rusefi_can_rx_registration_t* reg) {
	/** Add a handler to the fixed-size handler registry. */
	if (core == NULL || !is_valid_registration(reg)) {
		return -1;
	}

	if (core->handler_count >= (uint8_t)RUSEFI_CAN_CORE_MAX_HANDLERS) {
		return -2;
	}

	core->handlers[core->handler_count] = *reg;
	core->handler_count++;

	return 0;
}

// PUBLIC_INTERFACE
int rusefi_can_core_dispatch_rx(rusefi_can_core_t* core, const rusefi_can_frame_t* frame) {
	/** Dispatch a received frame to all matching registered handlers. */
	if (core == NULL || frame == NULL) {
		return 0;
	}

	int invoked = 0;

	for (uint8_t i = 0; i < core->handler_count; i++) {
		const rusefi_can_rx_registration_t* reg = &core->handlers[i];

		if (!frame_matches(reg, frame)) {
			continue;
		}

		reg->handler(reg->user_ctx, frame);
		invoked++;
	}

	return invoked;
}

// PUBLIC_INTERFACE
int rusefi_can_core_send(rusefi_can_core_t* core, const rusefi_can_frame_t* frame) {
	/** Send a frame via the configured TX interface. */
	if (core == NULL || frame == NULL) {
		return -1;
	}

	if (core->tx.send == NULL) {
		/* No TX configured. Keep explicit error to help integration. */
		return -2;
	}

	return core->tx.send(core->tx.user_ctx, frame);
}
