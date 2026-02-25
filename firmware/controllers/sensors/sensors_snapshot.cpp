#include "engine.h"
#include "sensors_snapshot.h"

#include "adc_math.h"
#include "adc_inputs.h"
#include "thermistors.h"

/*
 * Platform IO adapter callbacks
 *
 * These intentionally reuse existing firmware functions/macros so behavior of
 * existing callers remains unchanged (this is a new additive API).
 */

static float fw_read_voltage_divided(void* user_ctx, const char* name, int32_t hw_channel) {
	(void)user_ctx;
	return getVoltageDivided(name, (adc_channel_e)hw_channel);
}

static float fw_read_voltage(void* user_ctx, const char* name, int32_t hw_channel) {
	(void)user_ctx;
	return getVoltage(name, (adc_channel_e)hw_channel);
}

static int32_t fw_read_adc(void* user_ctx, const char* name, int32_t hw_channel) {
	(void)user_ctx;
	return getAdcValue(name, (adc_channel_e)hw_channel);
}

static rusefi_headless_thermistor_decode_config_t makeThermConfigFromFirmware(
	const thermistor_conf_s* tc,
	const ThermistorMath* curve,
	bool useLinear) {
	rusefi_headless_thermistor_decode_config_t out = {};
	out.vin = 5.0f;
	out.bias_resistor_ohms = tc->bias_resistor;

	if (useLinear) {
		out.mode = RUSEFI_HEADLESS_THERM_MODE_LINEAR_2POINT;

		/* Legacy linear mode: resistance_{1,2} store VOLTAGE endpoints */
		out.linear_v1 = tc->resistance_1;
		out.linear_t1_c = tc->tempC_1;

		out.linear_v2 = tc->resistance_2;
		out.linear_t2_c = tc->tempC_2;
	} else {
		out.mode = RUSEFI_HEADLESS_THERM_MODE_STEINHART_HART;

		/* Use already-prepared coefficients from the firmware curve helper */
		out.sh.s_h_a = curve->s_h_a;
		out.sh.s_h_b = curve->s_h_b;
		out.sh.s_h_c = curve->s_h_c;
	}

	return out;
}

// PUBLIC_INTERFACE
int readFirmwareSensorsSnapshot(firmware_sensors_snapshot_t* out DECLARE_ENGINE_PARAMETER_SUFFIX) {
	/**
	 * Read and decode key sensors using the headless snapshot API.
	 *
	 * This function does not emit warnings/OBD codes: it returns flags so callers
	 * can decide how to handle invalid data. Existing legacy sensor getters keep
	 * their original behavior.
	 */
	if (out == NULL) {
		return -1;
	}

	rusefi_headless_sensors_io_t io = {};
	io.user_ctx = NULL;
	io.read_voltage_divided = fw_read_voltage_divided;
	io.read_voltage = fw_read_voltage;
	io.read_adc = fw_read_adc;

	rusefi_headless_sensors_snapshot_config_t cfg = {};
	cfg.map_adc_channel = engineConfiguration->map.sensor.hwChannel;
	cfg.baro_adc_channel = engineConfiguration->baroSensor.hwChannel;
	cfg.tps_adc_channel = engineConfiguration->tpsAdcChannel;
	cfg.clt_adc_channel = engineConfiguration->clt.adcChannel;
	cfg.iat_adc_channel = engineConfiguration->iat.adcChannel;
	cfg.vbatt_adc_channel = engineConfiguration->vbattAdcChannel;

	/* MAP decode config (matches wrappers in map.cpp) */
	cfg.map_decode.type = (engineConfiguration->map.sensor.type == MT_CUSTOM)
		? RUSEFI_MAP_TYPE_CUSTOM_LINEAR
		: RUSEFI_MAP_TYPE_FALLBACK_0_5_LINEAR; /* will be overwritten below for known types */

	/* Map known types */
	switch (engineConfiguration->map.sensor.type) {
		case MT_CUSTOM:
			cfg.map_decode.type = RUSEFI_MAP_TYPE_CUSTOM_LINEAR;
			cfg.map_decode.low_voltage = engineConfiguration->mapLowValueVoltage;
			cfg.map_decode.high_voltage = engineConfiguration->mapHighValueVoltage;
			cfg.map_decode.low_kpa = engineConfiguration->map.sensor.lowValue;
			cfg.map_decode.high_kpa = engineConfiguration->map.sensor.highValue;
			break;
		case MT_DENSO183: cfg.map_decode.type = RUSEFI_MAP_TYPE_DENSO183; break;
		case MT_MPX4250: cfg.map_decode.type = RUSEFI_MAP_TYPE_MPX4250; break;
		case MT_MPX4250A: cfg.map_decode.type = RUSEFI_MAP_TYPE_MPX4250A; break;
		case MT_HONDA3BAR: cfg.map_decode.type = RUSEFI_MAP_TYPE_HONDA3BAR; break;
		case MT_DODGE_NEON_2003: cfg.map_decode.type = RUSEFI_MAP_TYPE_DODGE_NEON_2003; break;
		case MT_SUBY_DENSO: cfg.map_decode.type = RUSEFI_MAP_TYPE_SUBY_DENSO; break;
		case MT_GM_3_BAR: cfg.map_decode.type = RUSEFI_MAP_TYPE_GM_3_BAR; break;
		case MT_TOYOTA_89420_02010: cfg.map_decode.type = RUSEFI_MAP_TYPE_TOYOTA_89420_02010; break;
		case MT_MPX4100:
			/* Preserve historical behavior: unknown decoder fallback is 0..5V linear */
			cfg.map_decode.type = RUSEFI_MAP_TYPE_FALLBACK_0_5_LINEAR;
			cfg.map_decode.fallback_low_kpa = engineConfiguration->map.sensor.lowValue;
			cfg.map_decode.fallback_high_kpa = engineConfiguration->map.sensor.highValue;
			break;
		default:
			/* Unknown -> snapshot will return NAN + invalid flag */
			break;
	}

	/* Baro decode config mirrors MAP config mapping */
	cfg.baro_decode = cfg.map_decode;
	switch (engineConfiguration->baroSensor.type) {
		case MT_CUSTOM:
			cfg.baro_decode.type = RUSEFI_MAP_TYPE_CUSTOM_LINEAR;
			cfg.baro_decode.low_voltage = engineConfiguration->mapLowValueVoltage;
			cfg.baro_decode.high_voltage = engineConfiguration->mapHighValueVoltage;
			cfg.baro_decode.low_kpa = engineConfiguration->baroSensor.lowValue;
			cfg.baro_decode.high_kpa = engineConfiguration->baroSensor.highValue;
			break;
		case MT_DENSO183: cfg.baro_decode.type = RUSEFI_MAP_TYPE_DENSO183; break;
		case MT_MPX4250: cfg.baro_decode.type = RUSEFI_MAP_TYPE_MPX4250; break;
		case MT_MPX4250A: cfg.baro_decode.type = RUSEFI_MAP_TYPE_MPX4250A; break;
		case MT_HONDA3BAR: cfg.baro_decode.type = RUSEFI_MAP_TYPE_HONDA3BAR; break;
		case MT_DODGE_NEON_2003: cfg.baro_decode.type = RUSEFI_MAP_TYPE_DODGE_NEON_2003; break;
		case MT_SUBY_DENSO: cfg.baro_decode.type = RUSEFI_MAP_TYPE_SUBY_DENSO; break;
		case MT_GM_3_BAR: cfg.baro_decode.type = RUSEFI_MAP_TYPE_GM_3_BAR; break;
		case MT_TOYOTA_89420_02010: cfg.baro_decode.type = RUSEFI_MAP_TYPE_TOYOTA_89420_02010; break;
		case MT_MPX4100:
			cfg.baro_decode.type = RUSEFI_MAP_TYPE_FALLBACK_0_5_LINEAR;
			cfg.baro_decode.fallback_low_kpa = engineConfiguration->baroSensor.lowValue;
			cfg.baro_decode.fallback_high_kpa = engineConfiguration->baroSensor.highValue;
			break;
		default:
			break;
	}

	/* TPS decode config */
	cfg.tps_decode.tps_min_adc12 = engineConfiguration->tpsMin;
	cfg.tps_decode.tps_max_adc12 = engineConfiguration->tpsMax;
	cfg.tps_decode.error_detection_too_low = engineConfiguration->tpsErrorDetectionTooLow;
	cfg.tps_decode.error_detection_too_high = engineConfiguration->tpsErrorDetectionTooHigh;

	/* Thermistor decode config */
	cfg.clt_decode = makeThermConfigFromFirmware(&engineConfiguration->clt.config, &engine->engineState.cltCurve,
		engineConfiguration->useLinearCltSensor);
	cfg.iat_decode = makeThermConfigFromFirmware(&engineConfiguration->iat.config, &engine->engineState.iatCurve,
		engineConfiguration->useLinearIatSensor);

	/* Validation thresholds (match legacy functions in map.cpp/thermistors.cpp) */
	cfg.map_error_detection_too_low = engineConfiguration->mapErrorDetectionTooLow;
	cfg.map_error_detection_too_high = engineConfiguration->mapErrorDetectionTooHigh;

	cfg.clt_min_c = -50.0f;
	cfg.clt_max_c = 250.0f;

	cfg.iat_min_c = -50.0f;
	cfg.iat_max_c = 100.0f;

	cfg.vbatt_divider_coeff = engineConfiguration->vbattDividerCoeff;

	return rusefi_headless_read_sensors_snapshot(&io, &cfg, &out->snapshot);
}
