#include "rusefi_headless_sensors_snapshot.h"

#include <math.h>
#include <string.h>

/* Local helper: interpolation used only for legacy linear thermistor mode */
static float rusefi_headless_interpolate(float x1, float y1, float x2, float y2, float x) {
	if (x1 == x2) {
		return NAN;
	}

	/* a*x1 + b = y1, a*x2 + b = y2 */
	const float a = (y1 - y2) / (x1 - x2);
	const float b = y1 - a * x1;
	return a * x + b;
}

static float rusefi_headless_thermistor_resistance_from_vout(float vout, float vin, float bias_r_ohms) {
	/* vout = r2/(r1+r2)*vin => solve for r2 */
	return rusefi_headless_voltage_divider_r2(vout, vin, bias_r_ohms);
}

static float rusefi_headless_thermistor_celsius_from_voltage(
	float vout,
	const rusefi_headless_thermistor_decode_config_t* cfg) {
	if (cfg == NULL) {
		return NAN;
	}

	if (cfg->mode == RUSEFI_HEADLESS_THERM_MODE_LINEAR_2POINT) {
		/* Legacy linear mode: interpolate voltage->temperature */
		return rusefi_headless_interpolate(cfg->linear_v1, cfg->linear_t1_c, cfg->linear_v2, cfg->linear_t2_c, vout);
	}

	/* Default: Steinhart-Hart */
	const float r_ohms = rusefi_headless_thermistor_resistance_from_vout(vout, cfg->vin, cfg->bias_resistor_ohms);
	const float k = rusefi_headless_thermistor_kelvin_from_resistance(r_ohms, &cfg->sh);

	/* Convert Kelvin -> Celsius. Match firmware's constant (KELV=273.15f) */
	return k - 273.15f;
}

static int rusefi_headless_is_valid_range(float v, float min, float max) {
	return !isnan(v) && v >= min && v <= max;
}

static int rusefi_headless_io_present(const rusefi_headless_sensors_io_t* io) {
	return io != NULL
		&& io->read_voltage_divided != NULL
		&& io->read_voltage != NULL
		&& io->read_adc != NULL;
}

int rusefi_headless_read_sensors_snapshot(
	const rusefi_headless_sensors_io_t* io,
	const rusefi_headless_sensors_snapshot_config_t* cfg,
	rusefi_headless_sensors_snapshot_t* out) {
	if (cfg == NULL || out == NULL) {
		return -1;
	}

	memset(out, 0, sizeof(*out));
	out->map_kpa = NAN;
	out->baro_kpa = NAN;
	out->tps_percent = NAN;
	out->clt_c = NAN;
	out->iat_c = NAN;
	out->vbatt = NAN;

	if (!rusefi_headless_io_present(io)) {
		out->flags |= RUSEFI_HEADLESS_SNAPSHOT_FLAG_IO_MISSING;
		return -2;
	}

	/* Read raw values */
	out->map_voltage_divided = io->read_voltage_divided(io->user_ctx, "map", cfg->map_adc_channel);
	out->baro_voltage_divided = io->read_voltage_divided(io->user_ctx, "baro", cfg->baro_adc_channel);
	out->tps_adc = io->read_adc(io->user_ctx, "tps", cfg->tps_adc_channel);
	out->clt_voltage_divided = io->read_voltage_divided(io->user_ctx, "clt", cfg->clt_adc_channel);
	out->iat_voltage_divided = io->read_voltage_divided(io->user_ctx, "iat", cfg->iat_adc_channel);

	out->vbatt_voltage = io->read_voltage(io->user_ctx, "vbatt", cfg->vbatt_adc_channel);

	/* Decode MAP/Baro */
	out->map_kpa = rusefi_headless_decode_map_kpa(out->map_voltage_divided, &cfg->map_decode);
	out->baro_kpa = rusefi_headless_decode_map_kpa(out->baro_voltage_divided, &cfg->baro_decode);

	/* Decode TPS */
	{
		rusefi_tps_decode_result_t tps = rusefi_headless_decode_tps_percent(out->tps_adc, &cfg->tps_decode);
		out->tps_percent = tps.percent_clamped;
		if (tps.flags & RUSEFI_TPS_DECODE_FLAG_INVALID_CONFIG) {
			out->flags |= RUSEFI_HEADLESS_SNAPSHOT_FLAG_TPS_INVALID;
		}
		/* Note: TOO_LOW/TOO_HIGH are reported via tps.flags, but snapshot uses only INVALID for now. */
	}

	/* Decode thermistors */
	out->clt_c = rusefi_headless_thermistor_celsius_from_voltage(out->clt_voltage_divided, &cfg->clt_decode);
	out->iat_c = rusefi_headless_thermistor_celsius_from_voltage(out->iat_voltage_divided, &cfg->iat_decode);

	/* Decode VBatt using legacy scaling */
	out->vbatt = out->vbatt_voltage * cfg->vbatt_divider_coeff;

	/* Validate MAP */
	if (isnan(out->map_kpa)
		|| out->map_kpa < cfg->map_error_detection_too_low
		|| out->map_kpa > cfg->map_error_detection_too_high) {
		out->flags |= RUSEFI_HEADLESS_SNAPSHOT_FLAG_MAP_INVALID;
	}

	/* Validate Baro: no strict range here, just NAN check (platform may validate separately) */
	if (isnan(out->baro_kpa)) {
		out->flags |= RUSEFI_HEADLESS_SNAPSHOT_FLAG_BARO_INVALID;
	}

	/* Validate thermistor temperatures */
	if (!rusefi_headless_is_valid_range(out->clt_c, cfg->clt_min_c, cfg->clt_max_c)) {
		out->flags |= RUSEFI_HEADLESS_SNAPSHOT_FLAG_CLT_INVALID;
	}

	if (!rusefi_headless_is_valid_range(out->iat_c, cfg->iat_min_c, cfg->iat_max_c)) {
		out->flags |= RUSEFI_HEADLESS_SNAPSHOT_FLAG_IAT_INVALID;
	}

	/* Validate VBatt: just NAN check */
	if (isnan(out->vbatt)) {
		out->flags |= RUSEFI_HEADLESS_SNAPSHOT_FLAG_VBATT_INVALID;
	}

	return 0;
}
