#ifndef RUSEFI_HEADLESS_ESM_H
#define RUSEFI_HEADLESS_ESM_H

/**
 * @file rusefi_headless_esm.h
 * @brief Headless Engine State Management (ESM) module public C API (skeleton).
 *
 * This module will eventually own derived engine state (cranking/running, etc)
 * in a platform-independent manner. For now it is a compile-only scaffold.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rusefi_esm_ctx rusefi_esm_ctx_t;

/**
 * PUBLIC_INTERFACE
 * @brief Create ESM context.
 */
rusefi_esm_ctx_t* rusefi_esm_create(void);

/**
 * PUBLIC_INTERFACE
 * @brief Destroy ESM context.
 */
void rusefi_esm_destroy(rusefi_esm_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Start ESM (no-op stub for now).
 *
 * @return 0 on success, non-zero on failure.
 */
int rusefi_esm_start(rusefi_esm_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Stop ESM (no-op stub for now).
 */
void rusefi_esm_stop(rusefi_esm_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Provide current time (microseconds) and RPM sample for ESM updates.
 *
 * This signature is intentionally simple at skeleton stage.
 */
void rusefi_esm_update(rusefi_esm_ctx_t* ctx, uint64_t now_us, float rpm);

/**
 * PUBLIC_INTERFACE
 * @brief Retrieve last RPM value tracked by ESM (stub returns 0.0f).
 */
float rusefi_esm_get_rpm(const rusefi_esm_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HEADLESS_ESM_H */
