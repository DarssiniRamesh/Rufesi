#include "rusefi_headless_esm.h"

#include <stdlib.h>

struct rusefi_esm_ctx {
	/* Skeleton: no state yet */
	float rpm;
};

rusefi_esm_ctx_t* rusefi_esm_create(void) {
	return (rusefi_esm_ctx_t*)calloc(1, sizeof(rusefi_esm_ctx_t));
}

void rusefi_esm_destroy(rusefi_esm_ctx_t* ctx) {
	free(ctx);
}

int rusefi_esm_start(rusefi_esm_ctx_t* ctx) {
	(void)ctx;
	return 0;
}

void rusefi_esm_stop(rusefi_esm_ctx_t* ctx) {
	(void)ctx;
}

void rusefi_esm_update(rusefi_esm_ctx_t* ctx, uint64_t now_us, float rpm) {
	(void)now_us;
	if (ctx) {
		ctx->rpm = rpm;
	}
}

float rusefi_esm_get_rpm(const rusefi_esm_ctx_t* ctx) {
	return ctx ? ctx->rpm : 0.0f;
}
