#ifndef RUSEFI_HEADLESS_CAN_H
#define RUSEFI_HEADLESS_CAN_H

/**
 * @file rusefi_headless_can.h
 * @brief Headless CAN module public C API (skeleton).
 *
 * This API is intentionally minimal and does not change existing behavior yet.
 * The implementation is currently a no-op stub that compiles in all targets.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rusefi_can_ctx rusefi_can_ctx_t;

/**
 * PUBLIC_INTERFACE
 * @brief Initialize headless CAN module context.
 *
 * @return Non-NULL context pointer on success, NULL on allocation failure.
 */
rusefi_can_ctx_t* rusefi_can_create(void);

/**
 * PUBLIC_INTERFACE
 * @brief Destroy a headless CAN module context.
 */
void rusefi_can_destroy(rusefi_can_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Start CAN module operation (no-op stub for now).
 *
 * @return 0 on success, non-zero on failure.
 */
int rusefi_can_start(rusefi_can_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Stop CAN module operation (no-op stub for now).
 */
void rusefi_can_stop(rusefi_can_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Send a raw CAN frame (no-op stub for now).
 *
 * @param id  Standard/extended CAN ID (consumer defines interpretation).
 * @param data Pointer to payload bytes (may be NULL if dlc==0).
 * @param dlc  Payload length [0..8].
 * @return 0 on success, non-zero on failure/unsupported.
 */
int rusefi_can_send(rusefi_can_ctx_t* ctx, uint32_t id, const uint8_t* data, uint8_t dlc);

/**
 * PUBLIC_INTERFACE
 * @brief Poll module periodic work (no-op stub for now).
 *
 * Intended to be called from a scheduler in future refactor steps.
 */
void rusefi_can_poll(rusefi_can_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HEADLESS_CAN_H */
