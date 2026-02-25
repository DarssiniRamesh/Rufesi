/**
 * @file    voltage.cpp
 * @brief
 *
 *
 * @date Nov 15, 2013
 * @author Andrey Belomutskiy, (c) 2012-2018
 */

#include "global.h"
#include "engine.h"
#include "analog_input.h"
#include "voltage.h"

#include "sensors_snapshot.h"

EXTERN_ENGINE;

float getVRef(DECLARE_ENGINE_PARAMETER_SIGNATURE) {
	// not currently used	return getVoltageDivided("vref", engineConfiguration->vRefAdcChannel);
	return NAN;
}

bool hasVBatt(DECLARE_ENGINE_PARAMETER_SIGNATURE) {
	return engineConfiguration->vbattAdcChannel != EFI_ADC_NONE;
}

float getVBatt(DECLARE_ENGINE_PARAMETER_SIGNATURE) {
	/*
	 * Prefer the headless snapshot adapter (portable contract) to avoid direct sensor IO
	 * in new code paths, but preserve legacy behavior if snapshot cannot be read.
	 *
	 * Behavior preservation:
	 *  - Both paths ultimately read getVoltage("vbatt", channel) * vbattDividerCoeff.
	 *  - On any snapshot error, we fall back to the legacy direct read.
	 */
	firmware_sensors_snapshot_t fw = {};
	if (readFirmwareSensorsSnapshot(&fw PASS_ENGINE_PARAMETER_SUFFIX) == 0) {
		return fw.snapshot.vbatt;
	}

	return getVoltage("vbatt", engineConfiguration->vbattAdcChannel) * engineConfiguration->vbattDividerCoeff;
}
