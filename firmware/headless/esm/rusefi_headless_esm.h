#ifndef RUSEFI_HEADLESS_ESM_H
#define RUSEFI_HEADLESS_ESM_H

/**
 * @file rusefi_headless_esm.h
 * @brief Headless Engine State Machine (ESM) module public C API.
 *
 * Contract / purpose:
 *  - Own the engine "spinning state" (stopped/spinning-up/cranking/running) transition logic in a
 *    platform-independent, testable module.
 *  - This extraction specifically targets the mode transition/hysteresis logic previously embedded
 *    in firmware/controllers/trigger/rpm_calculator.cpp.
 *
 * Key invariants:
 *  - RUNNING is "sticky" with respect to RPM drops below cranking threshold (hysteresis),
 *    matching legacy behavior: once running, a drop below the cranking threshold does not
 *    immediately become CRANKING unless RPM reaches 0.
 *  - SPINNING_UP can be entered only from a fully stopped + non-spinning condition when
 *    "faster spin-up" feature is enabled.
 *
 * Side effects:
 *  - None. This module is pure state management with explicit inputs/outputs.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESM engine spinning states.
 *
 * Note: Values/names intentionally resemble the firmware's spinning_state_e, but remain
 * prefixed to avoid global enum symbol collisions across the codebase.
 */
typedef enum {
	RUSEFI_ESM_STOPPED = 0,
	RUSEFI_ESM_SPINNING_UP = 1,
	RUSEFI_ESM_CRANKING = 2,
	RUSEFI_ESM_RUNNING = 3,
} rusefi_esm_state_e;

/**
 * @brief ESM context (caller-owned storage).
 *
 * The context is intentionally a plain struct so firmware can embed it without heap allocation.
 */
typedef struct {
	rusefi_esm_state_e state;

	/**
	 * @brief Tracks whether the engine has started spinning (shaft motion observed).
	 *
	 * This is used to gate entry into SPINNING_UP so that only a completely stopped engine can
	 * enter SPINNING_UP, matching legacy rpm_calculator behavior.
	 */
	bool is_spinning;
} rusefi_esm_ctx_t;

/**
 * PUBLIC_INTERFACE
 * @brief Initialize/reset an ESM context to default state.
 *
 * Inputs:
 *  - ctx: non-null pointer to context storage
 *
 * Outputs:
 *  - ctx is set to STOPPED and not spinning
 *
 * Errors:
 *  - If ctx is NULL, the call is a no-op.
 */
void rusefi_esm_init(rusefi_esm_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Update ESM state based on a new RPM value, using the configured cranking RPM threshold.
 *
 * This implements the legacy transition logic previously in RpmCalculator::setRpmValue().
 *
 * Inputs:
 *  - ctx: non-null
 *  - rpm: current RPM (integer semantics, but passed as int for convenience)
 *  - cranking_rpm_threshold: threshold at/above which state becomes RUNNING
 *
 * Outputs:
 *  - ctx->state may change
 *
 * Return:
 *  - true if state changed, false otherwise
 */
bool rusefi_esm_on_rpm(rusefi_esm_ctx_t* ctx, int rpm, int cranking_rpm_threshold);

/**
 * PUBLIC_INTERFACE
 * @brief Request entry into SPINNING_UP if allowed by current conditions.
 *
 * Legacy mapping: RpmCalculator::setSpinningUp().
 *
 * Inputs:
 *  - ctx: non-null
 *  - faster_spin_up_enabled: feature flag (CONFIGB(isFasterEngineSpinUpEnabled))
 *
 * Outputs:
 *  - If enabled and (state is STOPPED) and (is_spinning==false), then:
 *      state=SPINNING_UP, is_spinning=true
 *
 * Return:
 *  - true if (after the call) the ctx is in SPINNING_UP state, false otherwise
 */
bool rusefi_esm_request_spinning_up(rusefi_esm_ctx_t* ctx, bool faster_spin_up_enabled);

/**
 * PUBLIC_INTERFACE
 * @brief Notify ESM that the engine stopped spinning (trigger timeout/loss of sync).
 *
 * Legacy mapping: RpmCalculator::setStopSpinning().
 *
 * Inputs:
 *  - ctx: non-null
 *
 * Outputs:
 *  - ctx->is_spinning is cleared
 *  - ctx->state becomes STOPPED
 */
void rusefi_esm_on_stop_spinning(rusefi_esm_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Accessors to mimic firmware-side predicates without duplicating logic.
 */
bool rusefi_esm_is_stopped(const rusefi_esm_ctx_t* ctx, int rpm);
bool rusefi_esm_is_cranking(const rusefi_esm_ctx_t* ctx, int rpm);
bool rusefi_esm_is_running(const rusefi_esm_ctx_t* ctx);
bool rusefi_esm_is_spinning_up(const rusefi_esm_ctx_t* ctx);
rusefi_esm_state_e rusefi_esm_get_state(const rusefi_esm_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HEADLESS_ESM_H */
