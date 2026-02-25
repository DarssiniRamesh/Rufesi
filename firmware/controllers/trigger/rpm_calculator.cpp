/**
 * @file    rpm_calculator.cpp
 * @brief   RPM calculator
 *
 * Here we listen to position sensor events in order to figure our if engine is currently running or not.
 * Actual getRpm() is calculated once per crankshaft revolution, based on the amount of time passed
 * since the start of previous shaft revolution.
 *
 * @date Jan 1, 2013
 * @author Andrey Belomutskiy, (c) 2012-2018
 */

#include "engine.h"
#include "rpm_calculator.h"

#include "trigger_central.h"
#include "engine_configuration.h"
#include "engine_math.h"

#if EFI_PROD_CODE
#include "rfiutil.h"
#include "engine.h"
#endif

#if EFI_SENSOR_CHART || defined(__DOXYGEN__)
#include "sensor_chart.h"
#endif

#include "efilib2.h"

#if EFI_ENGINE_SNIFFER
#include "engine_sniffer.h"
extern WaveChart waveChart;
#endif /* EFI_ENGINE_SNIFFER */

// See RpmCalculator::checkIfSpinning()
#ifndef NO_RPM_EVENTS_TIMEOUT_SECS
#define NO_RPM_EVENTS_TIMEOUT_SECS 2
#endif /* NO_RPM_EVENTS_TIMEOUT_SECS */

static spinning_state_e esmToLegacyState(rusefi_esm_state_e s) {
	switch (s) {
	case RUSEFI_ESM_STOPPED:
		return STOPPED;
	case RUSEFI_ESM_SPINNING_UP:
		return SPINNING_UP;
	case RUSEFI_ESM_CRANKING:
		return CRANKING;
	case RUSEFI_ESM_RUNNING:
	default:
		return RUNNING;
	}
}

float RpmCalculator::getRpmAcceleration() {
	return 1.0 * previousRpmValue / rpmValue;
}

bool RpmCalculator::isStopped(DECLARE_ENGINE_PARAMETER_SIGNATURE) const {
	// Spinning-up with zero RPM means that the engine is not ready yet, and is treated as 'stopped'.
	return rusefi_esm_is_stopped(&esm, rpmValue);
}

bool RpmCalculator::isCranking(DECLARE_ENGINE_PARAMETER_SIGNATURE) const {
	// Spinning-up with non-zero RPM is suitable for all engine math, as good as cranking
	return rusefi_esm_is_cranking(&esm, rpmValue);
}

bool RpmCalculator::isSpinningUp(DECLARE_ENGINE_PARAMETER_SIGNATURE) const {
	return rusefi_esm_is_spinning_up(&esm);
}

uint32_t RpmCalculator::getRevolutionCounterSinceStart(void) {
	return revolutionCounterSinceStart;
}

/**
 * @return -1 in case of isNoisySignal(), current RPM otherwise
 */
// todo: migrate to float return result or add a float version? this would have with calculations
int RpmCalculator::getRpm(DECLARE_ENGINE_PARAMETER_SIGNATURE) const {
#if !EFI_PROD_CODE
	if (mockRpm != MOCK_UNDEFINED) {
		return mockRpm;
	}
#endif /* EFI_PROD_CODE */
	return rpmValue;
}

#if EFI_SHAFT_POSITION_INPUT || defined(__DOXYGEN__)

EXTERN_ENGINE
;

extern bool hasFirmwareErrorFlag;

static Logging * logger;

int revolutionCounterSinceBootForUnitTest = 0;

RpmCalculator::RpmCalculator() {
#if !EFI_PROD_CODE
	mockRpm = MOCK_UNDEFINED;
#endif /* EFI_PROD_CODE */

	// Initialize headless ESM context
	rusefi_esm_init(&esm);

	// todo: reuse assignRpmValue() method which needs PASS_ENGINE_PARAMETER_SUFFIX
	// which we cannot provide inside this parameter-less constructor. need a solution for this minor mess

	// we need this initial to have not_running at first invocation
	lastRpmEventTimeNt = (efitime_t) -10 * US2NT(US_PER_SECOND_LL);
	revolutionCounterSinceBootForUnitTest = 0;
}

/**
 * @return true if there was a full shaft revolution within the last second
 */
bool RpmCalculator::isRunning(DECLARE_ENGINE_PARAMETER_SIGNATURE) const {
	return rusefi_esm_is_running(&esm);
}

/**
 * @return true if engine is spinning (cranking or running)
 */
static uint64_t fw_get_time_now_nt(void* user_ctx) {
	(void)user_ctx;
	return (uint64_t)getTimeNowNt();
}

void RpmCalculator::onEsmFastTick(efitick_t nowNt DECLARE_ENGINE_PARAMETER_SUFFIX) {
	(void)nowNt;

	rusefi_esm_deps_t deps = {};
	deps.user_ctx = NULL;
	deps.get_time_now_nt = fw_get_time_now_nt;

	/*
	 * IMPORTANT (behavior preservation + portability):
	 *  - Headless ESM fast/slow ticks are currently a no-op (they only record timestamps).
	 *  - Do not inject a firmware-only sensors snapshot getter here because this file is
	 *    also compiled in simulator/unit-test configurations where engine-parameter
	 *    macros change call signatures, which would break compilation.
	 */
	deps.get_sensors_snapshot = NULL;

	(void)rusefi_esm_fast_tick(&esm, &deps);
}

void RpmCalculator::onEsmSlowTick(efitick_t nowNt DECLARE_ENGINE_PARAMETER_SUFFIX) {
	(void)nowNt;

	rusefi_esm_deps_t deps = {};
	deps.user_ctx = NULL;
	deps.get_time_now_nt = fw_get_time_now_nt;
	deps.get_sensors_snapshot = NULL;

	(void)rusefi_esm_slow_tick(&esm, &deps);
}

bool RpmCalculator::checkIfSpinning(efitick_t nowNt DECLARE_ENGINE_PARAMETER_SUFFIX) const {
	if (ENGINE(needToStopEngine(nowNt))) {
		return false;
	}

	/**
	 * note that the result of this subtraction could be negative, that would happen if
	 * we have a trigger event between the time we've invoked 'getTimeNow' and here
	 */
	bool noRpmEventsForTooLong = nowNt - lastRpmEventTimeNt >= US2NT(NO_RPM_EVENTS_TIMEOUT_SECS * US_PER_SECOND_LL); // Anything below 60 rpm is not running
	/**
	 * Also check if there were no trigger events
	 */
	bool noTriggerEventsForTooLong = nowNt - engine->triggerCentral.previousShaftEventTimeNt >= US2NT(US_PER_SECOND_LL);
	if (noRpmEventsForTooLong || noTriggerEventsForTooLong) {
		return false;
	}

	return true;
}

void RpmCalculator::assignRpmValue(int value DECLARE_ENGINE_PARAMETER_SUFFIX) {
	previousRpmValue = rpmValue;
	rpmValue = value;
	if (rpmValue <= 0) {
		oneDegreeUs = NAN;
	} else {
		oneDegreeUs = getOneDegreeTimeUs(rpmValue);
		if (previousRpmValue == 0) {
			/**
			 * this would make sure that we have good numbers for first cranking revolution
			 * #275 cranking could be improved
			 */
			ENGINE(periodicFastCallback(PASS_ENGINE_PARAMETER_SIGNATURE));
		}
	}
}

void RpmCalculator::setRpmValue(int value DECLARE_ENGINE_PARAMETER_SUFFIX) {
	assignRpmValue(value PASS_ENGINE_PARAMETER_SUFFIX);

	spinning_state_e oldState = state;

	// Delegate mode transitions/hysteresis to headless ESM.
	bool changed = rusefi_esm_on_rpm(&esm, rpmValue, CONFIG(cranking.rpm));
	(void)changed;

	state = esmToLegacyState(rusefi_esm_get_state(&esm));

#if EFI_ENGINE_CONTROL || defined(__DOXYGEN__)
	// This presumably fixes injection mode change for cranking-to-running transition.
	// 'isSimultanious' flag should be updated for events if injection modes differ for cranking and running.
	if (state != oldState) {
		engine->injectionEvents.addFuelEvents(PASS_ENGINE_PARAMETER_SIGNATURE);
	}
#endif
}

spinning_state_e RpmCalculator::getState() const {
	return state;
}

void RpmCalculator::onNewEngineCycle() {
	revolutionCounterSinceBoot++;
	revolutionCounterSinceStart++;
#if EFI_UNIT_TEST
	revolutionCounterSinceBootForUnitTest = revolutionCounterSinceBoot;
#endif /* EFI_UNIT_TEST */
}

uint32_t RpmCalculator::getRevolutionCounter(void) {
	return revolutionCounterSinceBoot;
}

void RpmCalculator::setStopped(DECLARE_ENGINE_PARAMETER_SIGNATURE) {
	revolutionCounterSinceStart = 0;
	if (rpmValue != 0) {
		assignRpmValue(0 PASS_ENGINE_PARAMETER_SUFFIX);
		scheduleMsg(logger, "engine stopped");
	}

	// Keep ESM and legacy state aligned.
	rusefi_esm_on_rpm(&esm, 0, CONFIG(cranking.rpm));
	state = STOPPED;
}

void RpmCalculator::setStopSpinning(DECLARE_ENGINE_PARAMETER_SIGNATURE) {
	// Headless ESM owns the "is_spinning" flag and authoritative stop-spinning transition.
	rusefi_esm_on_stop_spinning(&esm);

	setStopped(PASS_ENGINE_PARAMETER_SIGNATURE);
}

void RpmCalculator::setSpinningUp(efitime_t nowNt DECLARE_ENGINE_PARAMETER_SUFFIX) {
	bool isInSpinUp = rusefi_esm_request_spinning_up(&esm, CONFIGB(isFasterEngineSpinUpEnabled));
	if (!isInSpinUp) {
		return;
	}

	// We just entered or remain in SPINNING_UP, update cached legacy state.
	state = SPINNING_UP;

	// Only on actual transition into spinning-up do we reset these firmware-side variables.
	// Legacy behavior: this happened when (isStopped && !isSpinning) became true.
	// We approximate by checking triggerState.spinningEventIndex reset should occur when it is currently zeroed?
	// Instead of guessing, we keep the old behavior by resetting unconditionally while in SPINNING_UP,
	// since legacy also did it on entry and then left it as-is.
	engine->triggerCentral.triggerState.spinningEventIndex = 0;

	// update variables needed by early instant RPM calc.
	engine->triggerCentral.triggerState.setLastEventTimeForInstantRpm(nowNt PASS_ENGINE_PARAMETER_SUFFIX);

	/**
	 * Update ignition pin indices if needed. Here we potentially switch to wasted spark temporarily.
	 */
	prepareIgnitionPinIndices(getCurrentIgnitionMode(PASS_ENGINE_PARAMETER_SIGNATURE) PASS_ENGINE_PARAMETER_SUFFIX);
}

/**
 * @brief Shaft position callback used by RPM calculation logic.
 *
 * This callback should always be the first of trigger callbacks because other callbacks depend of values
 * updated here.
 * This callback is invoked on interrupt thread.
 */
void rpmShaftPositionCallback(trigger_event_e ckpSignalType,
		uint32_t index DECLARE_ENGINE_PARAMETER_SUFFIX) {
	efitick_t nowNt = getTimeNowNt();
#if EFI_PROD_CODE
	efiAssertVoid(CUSTOM_ERR_6632, getRemainingStack(chThdGetSelfX()) > 256, "lowstckRCL");
#endif

	RpmCalculator *rpmState = &engine->rpmCalculator;

	if (index == 0) {
		ENGINE(m.beforeRpmCb) = GET_TIMESTAMP();

		bool hadRpmRecently = rpmState->checkIfSpinning(nowNt PASS_ENGINE_PARAMETER_SUFFIX);

		if (hadRpmRecently) {
			efitime_t diffNt = nowNt - rpmState->lastRpmEventTimeNt;
		/**
		 * Four stroke cycle is two crankshaft revolutions
		 *
		 * We always do '* 2' because the event signal is already adjusted to 'per engine cycle'
		 * and each revolution of crankshaft consists of two engine cycles revolutions
		 *
		 */
			if (diffNt == 0) {
				rpmState->setRpmValue(NOISY_RPM PASS_ENGINE_PARAMETER_SUFFIX);
			} else {
				int mult = (int)getEngineCycle(engineConfiguration->operationMode) / 360;
				int rpm = (int) (60 * US2NT(US_PER_SECOND_LL) * mult / diffNt);
				rpmState->setRpmValue(rpm > UNREALISTIC_RPM ? NOISY_RPM : rpm PASS_ENGINE_PARAMETER_SUFFIX);
			}
		}
		rpmState->onNewEngineCycle();
		rpmState->lastRpmEventTimeNt = nowNt;
		ENGINE(m.rpmCbTime) = GET_TIMESTAMP() - ENGINE(m.beforeRpmCb);
	}


#if EFI_SENSOR_CHART || defined(__DOXYGEN__)
	// this 'index==0' case is here so that it happens after cycle callback so
	// it goes into sniffer report into the first position
	if (ENGINE(sensorChartMode) == SC_TRIGGER) {
		angle_t crankAngle = getCrankshaftAngleNt(nowNt PASS_ENGINE_PARAMETER_SUFFIX);
		int signal = 1000 * ckpSignalType + index;
		scAddData(crankAngle, signal);
	}
#endif

	if (rpmState->isSpinningUp(PASS_ENGINE_PARAMETER_SIGNATURE)) {
		// we are here only once trigger is synchronized for the first time
		// while transitioning  from 'spinning' to 'running'
		// Replace 'normal' RPM with instant RPM for the initial spin-up period
		engine->triggerCentral.triggerState.movePreSynchTimestamps(PASS_ENGINE_PARAMETER_SIGNATURE);
		int prevIndex;
		int iRpm = engine->triggerCentral.triggerState.calculateInstantRpm(&prevIndex, nowNt PASS_ENGINE_PARAMETER_SUFFIX);
		// validate instant RPM - we shouldn't skip the cranking state
		iRpm = minI(iRpm, CONFIG(cranking.rpm) - 1);
		rpmState->assignRpmValue(iRpm PASS_ENGINE_PARAMETER_SUFFIX);
#if 0
		scheduleMsg(logger, "** RPM: idx=%d sig=%d iRPM=%d", index, ckpSignalType, iRpm);
#endif
	}
}

static scheduling_s tdcScheduler[2];

static char rpmBuffer[_MAX_FILLER];

#if (EFI_PROD_CODE || EFI_SIMULATOR) || defined(__DOXYGEN__)
/**
 * This callback has nothing to do with actual engine control, it just sends a Top Dead Center mark to the dev console
 * digital sniffer.
 */
static void onTdcCallback(void) {
	itoa10(rpmBuffer, GET_RPM());
	addEngineSnifferEvent(TOP_DEAD_CENTER_MESSAGE, (char* ) rpmBuffer);
}

/**
 * This trigger callback schedules the actual physical TDC callback in relation to trigger synchronization point.
 */
static void tdcMarkCallback(trigger_event_e ckpSignalType,
		uint32_t index0 DECLARE_ENGINE_PARAMETER_SUFFIX) {
	(void) ckpSignalType;
	bool isTriggerSynchronizationPoint = index0 == 0;
	if (isTriggerSynchronizationPoint && ENGINE(isEngineChartEnabled)) {
		int revIndex2 = engine->rpmCalculator.getRevolutionCounter() % 2;
		int rpm = GET_RPM();
		// todo: use tooth event-based scheduling, not just time-based scheduling
		if (isValidRpm(rpm)) {
			scheduleByAngle(rpm, &tdcScheduler[revIndex2], tdcPosition(),
						(schfunc_t) onTdcCallback, NULL, &engine->rpmCalculator);
		}
	}
}
#endif

#if EFI_PROD_CODE || EFI_SIMULATOR
int getRevolutionCounter() {
	return engine->rpmCalculator.getRevolutionCounter();
}
#endif

/**
 * @return Current crankshaft angle, 0 to 720 for four-stroke
 */
float getCrankshaftAngleNt(efitime_t timeNt DECLARE_ENGINE_PARAMETER_SUFFIX) {
	efitime_t timeSinceZeroAngleNt = timeNt
			- engine->rpmCalculator.lastRpmEventTimeNt;

	/**
	 * even if we use 'getOneDegreeTimeUs' macros here, it looks like the
	 * compiler is not smart enough to figure out that "A / ( B / C)" could be optimized into
	 * "A * C / B" in order to replace a slower division with a faster multiplication.
	 */
	int rpm = GET_RPM();
	return rpm == 0 ? NAN : timeSinceZeroAngleNt / getOneDegreeTimeNt(rpm);
}

void initRpmCalculator(Logging *sharedLogger DECLARE_ENGINE_PARAMETER_SUFFIX) {
	logger = sharedLogger;
	if (hasFirmwareError()) {
		return;
	}
#if (EFI_PROD_CODE || EFI_SIMULATOR) || defined(__DOXYGEN__)

	addTriggerEventListener(tdcMarkCallback, "chart TDC mark", engine);
#endif

	addTriggerEventListener(rpmShaftPositionCallback, "rpm reporter", engine);
}

#if (EFI_PROD_CODE || EFI_SIMULATOR) || defined(__DOXYGEN__)
/**
 * Schedules a callback 'angle' degree of crankshaft from now.
 * The callback would be executed once after the duration of time which
 * it takes the crankshaft to rotate to the specified angle.
 */
void scheduleByAngle(int rpm, scheduling_s *timer, angle_t angle,
		schfunc_t callback, void *param, RpmCalculator *calc DECLARE_ENGINE_PARAMETER_SUFFIX) {
	efiAssertVoid(CUSTOM_ANGLE_NAN, !cisnan(angle), "NaN angle?");
	efiAssertVoid(CUSTOM_ERR_6634, isValidRpm(rpm), "RPM check expected");
	float delayUs = calc->oneDegreeUs * angle;
	efiAssertVoid(CUSTOM_ERR_6635, !cisnan(delayUs), "NaN delay?");
	engine->executor.scheduleForLater(timer, (int) delayUs, callback, param);
}
#endif

#else
RpmCalculator::RpmCalculator() {

}

#endif /* EFI_SHAFT_POSITION_INPUT */
