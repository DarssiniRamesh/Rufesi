#include "rusefi_headless_can.h"

#include <stdlib.h>

struct rusefi_can_ctx {
	/* Intentionally empty for skeleton stage */
	int unused;
};

rusefi_can_ctx_t* rusefi_can_create(void) {
	return (rusefi_can_ctx_t*)calloc(1, sizeof(rusefi_can_ctx_t));
}

void rusefi_can_destroy(rusefi_can_ctx_t* ctx) {
	free(ctx);
}

int rusefi_can_start(rusefi_can_ctx_t* ctx) {
	(void)ctx;
	return 0;
}

void rusefi_can_stop(rusefi_can_ctx_t* ctx) {
	(void)ctx;
}

int rusefi_can_send(rusefi_can_ctx_t* ctx, uint32_t id, const uint8_t* data, uint8_t dlc) {
	(void)ctx;
	(void)id;
	(void)data;
	(void)dlc;
	/* Stub: accept call but do nothing */
	return 0;
}

void rusefi_can_poll(rusefi_can_ctx_t* ctx) {
	(void)ctx;
}
