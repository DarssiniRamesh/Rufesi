#ifndef CONTROLLERS_SENSORS_SENSORS_SNAPSHOT_H_
#define CONTROLLERS_SENSORS_SENSORS_SNAPSHOT_H_

/**
 * @file sensors_snapshot.h
 * @brief Firmware adapter around the headless sensors snapshot API.
 *
 * This file is additive: existing firmware sensor getters (getTPS/getRawMap/etc)
 * remain unchanged. This helper allows new code paths to fetch a consistent
 * decoded snapshot using the headless pure math helpers.
 */

#include "global.h"
#include "rusefi_headless_sensors_snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	rusefi_headless_sensors_snapshot_t snapshot;
} firmware_sensors_snapshot_t;

/**
 * PUBLIC_INTERFACE
 * @brief Fill a decoded/validated snapshot using current engineConfiguration and platform ADC sources.
 *
 * @param out Output snapshot container.
 * @return 0 on success, non-zero on error.
 */
int readFirmwareSensorsSnapshot(firmware_sensors_snapshot_t* out DECLARE_ENGINE_PARAMETER_SUFFIX);

#ifdef __cplusplus
}
#endif

#endif /* CONTROLLERS_SENSORS_SENSORS_SNAPSHOT_H_ */
