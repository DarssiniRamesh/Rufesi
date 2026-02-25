#include "rusefi_headless_sensors.h"

#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------------- */
/* Context API (still a stub)                                                 */
/* ------------------------------------------------------------------------- */

struct rusefi_sensors_ctx {
	/* Skeleton: store nothing yet */
	int unused;
};

rusefi_sensors_ctx_t* rusefi_sensors_create(void) {
	return (rusefi_sensors_ctx_t*)calloc(1, sizeof(rusefi_sensors_ctx_t));
}

void rusefi_sensors_destroy(rusefi_sensors_ctx_t* ctx) {
	free(ctx);
}

int rusefi_sensors_start(rusefi_sensors_ctx_t* ctx) {
	(void)ctx;
	return 0;
}

void rusefi_sensors_stop(rusefi_sensors_ctx_t* ctx) {
	(void)ctx;
}

void rusefi_sensors_set_raw(rusefi_sensors_ctx_t* ctx, uint16_t sensor_id, float value) {
	(void)ctx;
	(void)sensor_id;
	(void)value;
}

float rusefi_sensors_get(rusefi_sensors_ctx_t* ctx, uint16_t sensor_id) {
	(void)ctx;
	(void)sensor_id;
	return 0.0f;
}

void rusefi_sensors_poll(rusefi_sensors_ctx_t* ctx) {
	(void)ctx;
}

/* ------------------------------------------------------------------------- */
/* Pure decode math helpers                                                   */
/* ------------------------------------------------------------------------- */

static float rusefi_headless_interpolate(float x1, float y1, float x2, float y2, float x) {
	if (x1 == x2) {
		return NAN;
	}

	/* a*x1 + b = y1, a*x2 + b = y2 */
	float a = (y1 - y2) / (x1 - x2);
	float b = y1 - a * x1;
	return a * x + b;
}

static float rusefi_headless_fast_interp(float x, float x1, float y1, float x2, float y2) {
	return rusefi_headless_interpolate(x1, y1, x2, y2, x);
}

/* MAP fixed decoders replicated from firmware/controllers/sensors/map.cpp */
static float rusefi_map_denso183(float v) { return rusefi_headless_fast_interp(v, 0.0f, -6.64f, 5.0f, 182.78f); }
static float rusefi_map_honda3bar(float v) { return rusefi_headless_fast_interp(v, 0.5f, 91.422f, 3.0f, 0.0f); }
static float rusefi_map_suby_denso(float v) { return rusefi_headless_fast_interp(v, 0.0f, 0.0f, 5.0f, 200.0f); }
static float rusefi_map_gm3bar(float v) { return rusefi_headless_fast_interp(v, 0.631f, 40.0f, 4.914f, 304.0f); }
static float rusefi_map_mpx4250(float v) { return rusefi_headless_fast_interp(v, 0.0f, 8.0f, 5.0f, 260.0f); }
static float rusefi_map_mpx4250a(float v) { return rusefi_headless_fast_interp(v, 0.25f, 20.0f, 4.875f, 250.0f); }
static float rusefi_map_dodge_neon_2003(float v) { return rusefi_headless_fast_interp(v, 0.4f, 15.34f, 4.5f, 100.0f); }
static float rusefi_map_toyota_89420_02010(float v) {
	/* densoToyota(3.7 - 2, 33.322271, 3.7, 100) => (1.7, 33.322271) .. (3.7, 100) */
	return rusefi_headless_fast_interp(v, 1.7f, 33.322271f, 3.7f, 100.0f);
}

float rusefi_headless_decode_map_kpa(float voltage, const rusefi_map_config_t* cfg) {
	if (cfg == NULL) {
		return NAN;
	}

	switch (cfg->type) {
		case RUSEFI_MAP_TYPE_CUSTOM_LINEAR:
			return rusefi_headless_interpolate(cfg->low_voltage, cfg->low_kpa, cfg->high_voltage, cfg->high_kpa, voltage);

		case RUSEFI_MAP_TYPE_DENSO183:
			return rusefi_map_denso183(voltage);
		case RUSEFI_MAP_TYPE_MPX4250:
			return rusefi_map_mpx4250(voltage);
		case RUSEFI_MAP_TYPE_MPX4250A:
			return rusefi_map_mpx4250a(voltage);
		case RUSEFI_MAP_TYPE_HONDA3BAR:
			return rusefi_map_honda3bar(voltage);
		case RUSEFI_MAP_TYPE_DODGE_NEON_2003:
			return rusefi_map_dodge_neon_2003(voltage);
		case RUSEFI_MAP_TYPE_SUBY_DENSO:
			return rusefi_map_suby_denso(voltage);
		case RUSEFI_MAP_TYPE_GM_3_BAR:
			return rusefi_map_gm3bar(voltage);
		case RUSEFI_MAP_TYPE_TOYOTA_89420_02010:
			return rusefi_map_toyota_89420_02010(voltage);

		case RUSEFI_MAP_TYPE_FALLBACK_0_5_LINEAR:
			/* Legacy "unknown decoder" behavior: customMap.init(0, low, 5, high) */
			return rusefi_headless_interpolate(0.0f, cfg->fallback_low_kpa, 5.0f, cfg->fallback_high_kpa, voltage);

		default:
			return NAN;
	}
}

rusefi_tps_decode_result_t rusefi_headless_decode_tps_percent(int adc12, const rusefi_tps_config_t* cfg) {
	rusefi_tps_decode_result_t r = {
		.percent_unclamped = NAN,
		.percent_clamped = NAN,
		.flags = RUSEFI_TPS_DECODE_FLAG_NONE
	};

	if (cfg == NULL) {
		r.flags |= RUSEFI_TPS_DECODE_FLAG_INVALID_CONFIG;
		return r;
	}

	if (cfg->tps_min_adc12 == cfg->tps_max_adc12) {
		r.flags |= RUSEFI_TPS_DECODE_FLAG_INVALID_CONFIG;
		return r;
	}

	/*
	 * Legacy: interpolateMsg("TPS", TPS_TS_CONVERSION * tpsMax, 100, TPS_TS_CONVERSION * tpsMin, 0, adc);
	 * TPS_TS_CONVERSION is 4 (12-bit -> 10-bit TS scaling).
	 */
	const float x1 = 4.0f * (float)cfg->tps_max_adc12;
	const float y1 = 100.0f;
	const float x2 = 4.0f * (float)cfg->tps_min_adc12;
	const float y2 = 0.0f;
	const float x = (float)adc12;

	r.percent_unclamped = rusefi_headless_interpolate(x1, y1, x2, y2, x);

	if (!isnan(r.percent_unclamped)) {
		if (r.percent_unclamped < cfg->error_detection_too_low) {
			r.flags |= RUSEFI_TPS_DECODE_FLAG_TOO_LOW;
		}
		if (r.percent_unclamped > cfg->error_detection_too_high) {
			r.flags |= RUSEFI_TPS_DECODE_FLAG_TOO_HIGH;
		}

		/* Clamp to 0..100 */
		float clamped = r.percent_unclamped;
		if (clamped < 0.0f) clamped = 0.0f;
		if (clamped > 100.0f) clamped = 100.0f;
		r.percent_clamped = clamped;
	}

	return r;
}

int rusefi_headless_steinhart_hart_from_3points(
	float t1_kelvin, float r1,
	float t2_kelvin, float r2,
	float t3_kelvin, float r3,
	rusefi_steinhart_hart_coeffs_t* out) {

	if (out == NULL) {
		return -1;
	}

	if (t1_kelvin <= 0 || t2_kelvin <= 0 || t3_kelvin <= 0) {
		return -2;
	}
	if (r1 <= 0 || r2 <= 0 || r3 <= 0) {
		return -3;
	}

	/* Replicate ThermistorMath::prepareThermistorCurve math exactly */
	float L1 = logf(r1);
	float L2 = logf(r2);
	float L3 = logf(r3);

	float Y1 = 1.0f / t1_kelvin;
	float Y2 = 1.0f / t2_kelvin;
	float Y3 = 1.0f / t3_kelvin;

	float U2 = (Y2 - Y1) / (L2 - L1);
	float U3 = (Y3 - Y1) / (L3 - L1);

	float denom = (L1 + L2 + L3);
	if (denom == 0.0f) {
		return -4;
	}

	out->s_h_c = (U3 - U2) / (L3 - L2) * powf(denom, -1.0f);
	out->s_h_b = U2 - out->s_h_c * (L1 * L1 + L1 * L2 + L2 * L2);
	out->s_h_a = Y1 - (out->s_h_b + L1 * L1 * out->s_h_c) * L1;

	return 0;
}

float rusefi_headless_thermistor_kelvin_from_resistance(float resistance, const rusefi_steinhart_hart_coeffs_t* coeffs) {
	if (coeffs == NULL) {
		return 0.0f;
	}

	if (resistance <= 0) {
		/* Legacy behavior: return 0.0f for invalid resistance */
		return 0.0f;
	}

	float logR = logf(resistance);
	return 1.0f / (coeffs->s_h_a + coeffs->s_h_b * logR + coeffs->s_h_c * logR * logR * logR);
}

float rusefi_headless_voltage_divider_r1(float vout, float vin, float r2) {
	/* Legacy getR1InVoltageDividor had no explicit vout==0 check */
	return r2 * vin / vout - r2;
}

float rusefi_headless_voltage_divider_r2(float vout, float vin, float r1) {
	if (vout == 0) {
		return NAN;
	}

	return r1 / (vin / vout - 1.0f);
}

float rusefi_headless_voltage_divider_vout(float vin, float r1, float r2) {
	return r2 * vin / (r1 + r2);
}
