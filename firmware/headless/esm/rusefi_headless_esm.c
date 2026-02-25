#include "rusefi_headless_esm.h"

static rusefi_esm_state_e normalize_state(const rusefi_esm_ctx_t* ctx) {
	return ctx ? ctx->state : RUSEFI_ESM_STOPPED;
}

void rusefi_esm_init(rusefi_esm_ctx_t* ctx) {
	if (!ctx) {
		return;
	}

	ctx->state = RUSEFI_ESM_STOPPED;
	ctx->is_spinning = false;
}

bool rusefi_esm_on_rpm(rusefi_esm_ctx_t* ctx, int rpm, int cranking_rpm_threshold) {
	if (!ctx) {
		return false;
	}

	rusefi_esm_state_e oldState = ctx->state;

	// Legacy behavior from RpmCalculator::setRpmValue():
	// - rpm==0 -> STOPPED
	// - rpm>=cranking.rpm -> RUNNING
	// - else if (state==STOPPED || state==SPINNING_UP) -> CRANKING
	// - else keep existing (RUNNING remains RUNNING on RPM drop; CRANKING stays CRANKING)
	if (rpm == 0) {
		ctx->state = RUSEFI_ESM_STOPPED;
	} else if (rpm >= cranking_rpm_threshold) {
		ctx->state = RUSEFI_ESM_RUNNING;
	} else if (ctx->state == RUSEFI_ESM_STOPPED || ctx->state == RUSEFI_ESM_SPINNING_UP) {
		ctx->state = RUSEFI_ESM_CRANKING;
	}

	return ctx->state != oldState;
}

bool rusefi_esm_request_spinning_up(rusefi_esm_ctx_t* ctx, bool faster_spin_up_enabled) {
	if (!ctx) {
		return false;
	}

	if (!faster_spin_up_enabled) {
		return (ctx->state == RUSEFI_ESM_SPINNING_UP);
	}

	// Only a completely stopped and non-spinning engine can enter SPINNING_UP.
	if (ctx->state == RUSEFI_ESM_STOPPED && !ctx->is_spinning) {
		ctx->state = RUSEFI_ESM_SPINNING_UP;
		ctx->is_spinning = true;
	}

	return (ctx->state == RUSEFI_ESM_SPINNING_UP);
}

void rusefi_esm_on_stop_spinning(rusefi_esm_ctx_t* ctx) {
	if (!ctx) {
		return;
	}

	ctx->is_spinning = false;
	ctx->state = RUSEFI_ESM_STOPPED;
}

bool rusefi_esm_is_stopped(const rusefi_esm_ctx_t* ctx, int rpm) {
	rusefi_esm_state_e s = normalize_state(ctx);

	// Legacy behavior:
	// - state==STOPPED -> stopped
	// - state==SPINNING_UP && rpm==0 -> stopped (not ready yet)
	return (s == RUSEFI_ESM_STOPPED) || (s == RUSEFI_ESM_SPINNING_UP && rpm == 0);
}

bool rusefi_esm_is_cranking(const rusefi_esm_ctx_t* ctx, int rpm) {
	rusefi_esm_state_e s = normalize_state(ctx);

	// Legacy behavior:
	// - state==CRANKING -> cranking
	// - state==SPINNING_UP && rpm>0 -> suitable for engine math, treat as cranking
	return (s == RUSEFI_ESM_CRANKING) || (s == RUSEFI_ESM_SPINNING_UP && rpm > 0);
}

bool rusefi_esm_is_running(const rusefi_esm_ctx_t* ctx) {
	return normalize_state(ctx) == RUSEFI_ESM_RUNNING;
}

bool rusefi_esm_is_spinning_up(const rusefi_esm_ctx_t* ctx) {
	return normalize_state(ctx) == RUSEFI_ESM_SPINNING_UP;
}

rusefi_esm_state_e rusefi_esm_get_state(const rusefi_esm_ctx_t* ctx) {
	return normalize_state(ctx);
}
