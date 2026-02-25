#ifndef RUSEFI_HEADLESS_SENSORS_SNAPSHOT_H
#define RUSEFI_HEADLESS_SENSORS_SNAPSHOT_H

/**
 * @file rusefi_headless_sensors_snapshot.h
 * @brief Headless sensor snapshot API: platform IO adapter + decode/validate flow.
 *
 * This API is intended to be used by firmware/simulator/unit tests to:
 *  1) Read raw values from platform-provided ADC/voltage sources (via vtable),
 *  2) Decode those raw values using the existing headless pure math helpers, and
 *  3) Return decoded values plus validation flags.
 *
 * IMPORTANT:
 *  - This module must remain platform-agnostic: no logging, no firmwareError(), no warning().
 *  - Existing firmware sensor APIs/callers remain unchanged; this is additive.
 */

#include <stdint.h>

#include "rusefi_headless_sensors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Platform IO vtable                                                        */
/* ------------------------------------------------------------------------- */

typedef float (*rusefi_headless_read_voltage_f)(void* user_ctx, const char* name, int32_t hw_channel);
typedef int32_t (*rusefi_headless_read_adc_f)(void* user_ctx, const char* name, int32_t hw_channel);

typedef struct {
	/** User-provided context passed to all callbacks. */
	void* user_ctx;

	/**
	 * Read a voltage at an ADC input AFTER the analog input divider coefficient
	 * has been applied (i.e. matches rusEFI getVoltageDivided()).
	 */
	rusefi_headless_read_voltage_f read_voltage_divided;

	/**
	 * Read a voltage at an ADC input BEFORE analog input divider scaling
	 * (i.e. matches rusEFI getVoltage()).
	 */
	rusefi_headless_read_voltage_f read_voltage;

	/** Read raw ADC counts (typically 12-bit) (i.e. matches rusEFI getAdcValue()). */
	rusefi_headless_read_adc_f read_adc;
} rusefi_headless_sensors_io_t;

/* ------------------------------------------------------------------------- */
/* Config structures                                                          */
/* ------------------------------------------------------------------------- */

typedef enum {
	RUSEFI_HEADLESS_THERM_MODE_STEINHART_HART = 0,
	RUSEFI_HEADLESS_THERM_MODE_LINEAR_2POINT = 1
} rusefi_headless_therm_mode_e;

typedef struct {
	/**
	 * Voltage divider parameters. For rusEFI thermistors:
	 *   Vin is typically 5V, r1 is the bias resistor, r2 is the thermistor.
	 */
	float vin;
	float bias_resistor_ohms;

	rusefi_headless_therm_mode_e mode;

	/* Mode=STEINHART_HART */
	rusefi_steinhart_hart_coeffs_t sh;

	/**
	 * Mode=LINEAR_2POINT
	 *
	 * Legacy rusEFI linear mode historically used thermistorConf.resistance_{1,2}
	 * fields to store VOLTAGE values, and tempC_{1,2} for the endpoints.
	 *
	 * This headless config matches that behavior exactly: v1/v2 are voltages.
	 */
	float linear_v1;
	float linear_t1_c;
	float linear_v2;
	float linear_t2_c;
} rusefi_headless_thermistor_decode_config_t;

typedef struct {
	/* Hardware channels (platform-specific numeric ids) */
	int32_t map_adc_channel;
	int32_t baro_adc_channel;
	int32_t tps_adc_channel;
	int32_t clt_adc_channel;
	int32_t iat_adc_channel;
	int32_t vbatt_adc_channel;

	/* Decode config */
	rusefi_map_config_t map_decode;
	rusefi_map_config_t baro_decode;
	rusefi_tps_config_t tps_decode;

	rusefi_headless_thermistor_decode_config_t clt_decode;
	rusefi_headless_thermistor_decode_config_t iat_decode;

	/* Validation thresholds (pure math; platform decides what to do with invalid flags) */
	float map_error_detection_too_low;
	float map_error_detection_too_high;

	float clt_min_c;
	float clt_max_c;

	float iat_min_c;
	float iat_max_c;

	/* Battery voltage scaling (legacy firmware does getVoltage()*vbattDividerCoeff) */
	float vbatt_divider_coeff;
} rusefi_headless_sensors_snapshot_config_t;

/* ------------------------------------------------------------------------- */
/* Snapshot result                                                            */
/* ------------------------------------------------------------------------- */

typedef enum {
	RUSEFI_HEADLESS_SNAPSHOT_FLAG_NONE = 0,

	RUSEFI_HEADLESS_SNAPSHOT_FLAG_MAP_INVALID = (1u << 0),
	RUSEFI_HEADLESS_SNAPSHOT_FLAG_BARO_INVALID = (1u << 1),
	RUSEFI_HEADLESS_SNAPSHOT_FLAG_TPS_INVALID = (1u << 2),

	RUSEFI_HEADLESS_SNAPSHOT_FLAG_CLT_INVALID = (1u << 3),
	RUSEFI_HEADLESS_SNAPSHOT_FLAG_IAT_INVALID = (1u << 4),

	RUSEFI_HEADLESS_SNAPSHOT_FLAG_VBATT_INVALID = (1u << 5),

	RUSEFI_HEADLESS_SNAPSHOT_FLAG_IO_MISSING = (1u << 15)
} rusefi_headless_sensors_snapshot_flags_e;

typedef struct {
	/* Raw readings */
	float map_voltage_divided;
	float baro_voltage_divided;
	int32_t tps_adc;
	float clt_voltage_divided;
	float iat_voltage_divided;
	float vbatt_voltage; /* raw (not multiplied by divider_coeff) */

	/* Decoded values */
	float map_kpa;
	float baro_kpa;
	float tps_percent; /* 0..100 clamped */
	float clt_c;
	float iat_c;
	float vbatt; /* engineering units, volts */

	/* Bitmask of rusefi_headless_sensors_snapshot_flags_e */
	uint32_t flags;
} rusefi_headless_sensors_snapshot_t;

/**
 * PUBLIC_INTERFACE
 * @brief Read/Decode/Validate a consistent sensor snapshot using a platform IO vtable.
 *
 * @param io   Platform callbacks for reading ADC/voltage.
 * @param cfg  Snapshot configuration (channels + decode/validate thresholds).
 * @param out  Output snapshot.
 *
 * @return 0 on success. Non-zero on invalid parameters (io/cfg/out).
 */
int rusefi_headless_read_sensors_snapshot(
	const rusefi_headless_sensors_io_t* io,
	const rusefi_headless_sensors_snapshot_config_t* cfg,
	rusefi_headless_sensors_snapshot_t* out);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HEADLESS_SENSORS_SNAPSHOT_H */
