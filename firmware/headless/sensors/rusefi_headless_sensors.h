#ifndef RUSEFI_HEADLESS_SENSORS_H
#define RUSEFI_HEADLESS_SENSORS_H

/**
 * @file rusefi_headless_sensors.h
 * @brief Headless Sensors module public C API.
 *
 * This module provides:
 *  - A (currently stub) context-based sensors module API, and
 *  - Pure, platform-independent sensor decode math helpers (MAP/TPS/thermistor).
 *
 * The pure helpers are intended to be callable from firmware, unit tests, and
 * simulator builds without pulling in platform logging/error infrastructure.
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

/* ------------------------------------------------------------------------- */
/* Pure decode math helpers                                                   */
/* ------------------------------------------------------------------------- */

/**
 * A minimal map sensor type enum for headless decode math.
 *
 * Note: this intentionally does NOT reuse firmware enums, so call sites must
 * map types explicitly (wrappers preserve legacy behavior).
 */
typedef enum {
	RUSEFI_MAP_TYPE_CUSTOM_LINEAR = 0,
	RUSEFI_MAP_TYPE_DENSO183,
	RUSEFI_MAP_TYPE_MPX4250,
	RUSEFI_MAP_TYPE_MPX4250A,
	RUSEFI_MAP_TYPE_HONDA3BAR,
	RUSEFI_MAP_TYPE_DODGE_NEON_2003,
	RUSEFI_MAP_TYPE_SUBY_DENSO,
	RUSEFI_MAP_TYPE_GM_3_BAR,
	RUSEFI_MAP_TYPE_TOYOTA_89420_02010,

	/**
	 * Fallback: for "unknown decoder" behavior where legacy code fell back to a
	 * 0..5V linear interpolation between low/high values (see wrappers).
	 */
	RUSEFI_MAP_TYPE_FALLBACK_0_5_LINEAR = 100
} rusefi_map_type_e;

typedef struct {
	rusefi_map_type_e type;

	/* For CUSTOM_LINEAR: mapping between voltage and kPa */
	float low_voltage;
	float high_voltage;
	float low_kpa;
	float high_kpa;

	/* For FALLBACK_0_5_LINEAR: mapping between 0..5V and low/high kPa */
	float fallback_low_kpa;
	float fallback_high_kpa;
} rusefi_map_config_t;

/**
 * PUBLIC_INTERFACE
 * @brief Decode MAP/Baro sensor voltage to kPa using headless math.
 *
 * @param voltage Input voltage.
 * @param cfg     Decode configuration (type + parameters).
 *
 * @return kPa, or NAN on invalid input/config/type.
 */
float rusefi_headless_decode_map_kpa(float voltage, const rusefi_map_config_t* cfg);

/* ---- TPS ---- */

typedef struct {
	/* Raw min/max in 12-bit ADC counts (rusEFI legacy config uses 12-bit) */
	int tps_min_adc12;
	int tps_max_adc12;

	/* Error detection thresholds are compared against the *unclamped* percent */
	float error_detection_too_low;
	float error_detection_too_high;
} rusefi_tps_config_t;

typedef enum {
	RUSEFI_TPS_DECODE_FLAG_NONE = 0,
	RUSEFI_TPS_DECODE_FLAG_INVALID_CONFIG = (1u << 0),
	RUSEFI_TPS_DECODE_FLAG_TOO_LOW = (1u << 1),
	RUSEFI_TPS_DECODE_FLAG_TOO_HIGH = (1u << 2)
} rusefi_tps_decode_flags_e;

typedef struct {
	/* Unclamped percent (may be outside 0..100) */
	float percent_unclamped;

	/* Clamped 0..100 percent */
	float percent_clamped;

	/* Combination of rusefi_tps_decode_flags_e */
	uint32_t flags;
} rusefi_tps_decode_result_t;

/**
 * PUBLIC_INTERFACE
 * @brief Decode TPS based on configured min/max ADC counts and a raw ADC value.
 *
 * This matches the legacy rusEFI behavior:
 *   interpolate( tpsMax -> 100, tpsMin -> 0 ) using 10-bit TS scaling (x4).
 *
 * @param adc12 Raw 12-bit ADC reading.
 * @param cfg   TPS configuration.
 *
 * @return Decode result, including flags indicating invalid config/out-of-range.
 */
rusefi_tps_decode_result_t rusefi_headless_decode_tps_percent(int adc12, const rusefi_tps_config_t* cfg);

/* ---- Thermistor / voltage divider ---- */

typedef struct {
	/* Steinhart-Hart coefficients */
	float s_h_a;
	float s_h_b;
	float s_h_c;
} rusefi_steinhart_hart_coeffs_t;

/**
 * PUBLIC_INTERFACE
 * @brief Compute Steinhart-Hart coefficients from three (T,R) points.
 *
 * Temperatures must be in Kelvin. Resistances must be positive.
 *
 * @return 0 on success, non-zero on error.
 */
int rusefi_headless_steinhart_hart_from_3points(
	float t1_kelvin, float r1,
	float t2_kelvin, float r2,
	float t3_kelvin, float r3,
	rusefi_steinhart_hart_coeffs_t* out);

/**
 * PUBLIC_INTERFACE
 * @brief Convert thermistor resistance to temperature in Kelvin using Steinhart-Hart coefficients.
 *
 * @return Temperature in Kelvin, or 0.0f if resistance is invalid (<= 0) to match legacy behavior.
 */
float rusefi_headless_thermistor_kelvin_from_resistance(float resistance, const rusefi_steinhart_hart_coeffs_t* coeffs);

/**
 * PUBLIC_INTERFACE
 * @brief Compute R1 in a voltage divider: Vout = r2/(r1+r2)*Vin.
 */
float rusefi_headless_voltage_divider_r1(float vout, float vin, float r2);

/**
 * PUBLIC_INTERFACE
 * @brief Compute R2 in a voltage divider: Vout = r2/(r1+r2)*Vin.
 *
 * @return NAN if vout is 0.
 */
float rusefi_headless_voltage_divider_r2(float vout, float vin, float r1);

/**
 * PUBLIC_INTERFACE
 * @brief Compute Vout in a voltage divider: Vout = r2/(r1+r2)*Vin.
 */
float rusefi_headless_voltage_divider_vout(float vin, float r1, float r2);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HEADLESS_SENSORS_H */
