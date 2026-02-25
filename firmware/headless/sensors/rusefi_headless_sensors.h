#ifndef RUSEFI_HEADLESS_SENSORS_H
#define RUSEFI_HEADLESS_SENSORS_H

/**
 * @file rusefi_headless_sensors.h
 * @brief Headless Sensors module public C API (skeleton).
 *
 * This module will eventually encapsulate sensor acquisition, filtering, and
 * derived values independent of platform/firmware glue. For now it's a stub.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rusefi_sensors_ctx rusefi_sensors_ctx_t;

/**
 * PUBLIC_INTERFACE
 * @brief Create headless sensors module context.
 */
rusefi_sensors_ctx_t* rusefi_sensors_create(void);

/**
 * PUBLIC_INTERFACE
 * @brief Destroy headless sensors module context.
 */
void rusefi_sensors_destroy(rusefi_sensors_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Initialize/activate sensors module (no-op stub for now).
 *
 * @return 0 on success, non-zero on failure.
 */
int rusefi_sensors_start(rusefi_sensors_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Stop sensors module (no-op stub for now).
 */
void rusefi_sensors_stop(rusefi_sensors_ctx_t* ctx);

/**
 * PUBLIC_INTERFACE
 * @brief Provide a raw sensor sample to the module.
 *
 * @param sensor_id Caller-defined numeric id (future enum).
 * @param value     Sample value (raw or engineering units, TBD).
 */
void rusefi_sensors_set_raw(rusefi_sensors_ctx_t* ctx, uint16_t sensor_id, float value);

/**
 * PUBLIC_INTERFACE
 * @brief Retrieve the latest processed sensor value (stub returns 0.0f if unknown).
 */
float rusefi_sensors_get(rusefi_sensors_ctx_t* ctx, uint16_t sensor_id);

/**
 * PUBLIC_INTERFACE
 * @brief Periodic processing hook (no-op stub for now).
 */
void rusefi_sensors_poll(rusefi_sensors_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HEADLESS_SENSORS_H */
