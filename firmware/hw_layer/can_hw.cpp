/**
 * @file	can_hw.cpp
 * @brief	CAN bus platform glue (STM32/ChibiOS + rusEFI platform integration).
 *
 * Step 01.04 refactor:
 *  - Keep STM32/ChibiOS-specific IO (detectCanDevice/hal_can_* and threading) here.
 *  - Move message encoding/composition to can_messages.cpp
 *  - Perform RX dispatch and TX via the portable CAN core API (rusefi_can_core).
 *
 * Behavior should remain unchanged.
 *
 * @date Dec 11, 2013
 * @author Andrey Belomutskiy, (c) 2012-2018
 */

#include "global.h"

#if EFI_CAN_SUPPORT || defined(__DOXYGEN__)

#include "engine_configuration.h"
#include "pin_repository.h"
#include "can_hw.h"
#include "string.h"
#include "obd2.h"
#include "mpu_util.h"
#include "hal/hal_can.h"

#include "headless/can/rusefi_can_core.h"

EXTERN_ENGINE
;

static int canReadCounter = 0;
static int canWriteOk = 0;
static int canWriteNotOk = 0;
static bool isCanEnabled = false;
static LoggingWithStorage logger("CAN driver");
static THD_WORKING_AREA(canTreadStack, UTILITY_THREAD_STACK_SIZE);

/*
 * 500KBaud
 * automatic wakeup
 * automatic recover from abort mode
 * See section 22.7.7 on the STM32 reference manual.
 *
 * speed = 42000000 / (BRP + 1) / (1 + TS1 + 1 + TS2 + 1)
 * 42000000 / 7 / 12 = 500000
 */
static const CANConfig canConfig500 = {
	CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
	CAN_BTR_SJW(0) | CAN_BTR_TS2(1) | CAN_BTR_TS1(8) | CAN_BTR_BRP(6)
};

/*
 * speed = 42000000 / (BRP + 1) / (1 + TS1 + 1 + TS2 + 1)
 * 42000000 / 7 / 6 = 1000000
 */
static const CANConfig canConfig1000 = {
	CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
	CAN_BTR_SJW(0) | CAN_BTR_TS2(1) | CAN_BTR_TS1(2) | CAN_BTR_BRP(6)
};

// 42000000 / 14 / 12 = 250000
// todo: validate this
static const CANConfig canConfig250 = {
	CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
	CAN_BTR_SJW(0) | CAN_BTR_TS2(1) | CAN_BTR_TS1(8) | CAN_BTR_BRP(13)
};

static CANRxFrame rxBuffer;

/* Portable CAN core instance for dispatch + TX indirection. */
static rusefi_can_core_t s_can_core;

/* message encoder provides this */
extern void canInfoNBCBroadcast(can_nbc_e typeOfNBC);

/**
 * PUBLIC_INTERFACE
 * @brief Provide access to the configured portable CAN core instance.
 *
 * Contract:
 *  - Returns NULL until initCan() has enabled CAN and initialized the portable core/tx iface.
 *  - Callers must not retain the pointer across deinit/reinit cycles.
 *
 * This is used by can_messages.cpp/obd2.cpp to send frames via rusefi_can_core_send()
 * without including platform-specific IO.
 */
rusefi_can_core_t* canGetCore(void) {
	/* Preserve legacy behavior: when CAN is disabled/not initialized, TX calls are dropped. */
	return isCanEnabled ? &s_can_core : NULL;
}

static uint8_t rx_flags_from_chibios(const CANRxFrame* rx) {
	uint8_t flags = 0;

	if (rx->IDE == CAN_IDE_EXT) {
		flags |= RUSEFI_CAN_FRAME_FLAG_EXT;
	}
	if (rx->RTR == CAN_RTR_REMOTE) {
		flags |= RUSEFI_CAN_FRAME_FLAG_RTR;
	}

	return flags;
}

static uint32_t rx_id_from_chibios(const CANRxFrame* rx) {
	if (rx->IDE == CAN_IDE_EXT) {
		return (uint32_t)rx->EID;
	}
	return (uint32_t)rx->SID;
}

/**
 * RX handler: preserve legacy "printPacket" logging behavior.
 */
static void can_log_rx_handler(void* user_ctx, const rusefi_can_frame_t* frame) {
	(void)user_ctx;

	scheduleMsg(&logger,
		"Got CAN message: SID %x/%x %x %x %x %x %x %x %x %x",
		(unsigned int)frame->id,
		(unsigned int)frame->dlc,
		frame->data[0], frame->data[1], frame->data[2], frame->data[3],
		frame->data[4], frame->data[5], frame->data[6], frame->data[7]
	);

	if (frame->id == CAN_BMW_E46_CLUSTER_STATUS) {
		int odometerKm = 10 * (frame->data[1] << 8) + frame->data[0];
		int odometerMi = (int)(odometerKm * 0.621371);
		scheduleMsg(&logger, "GOT odometerKm %d", odometerKm);
		scheduleMsg(&logger, "GOT odometerMi %d", odometerMi);
		int timeValue = (frame->data[4] << 8) + frame->data[3];
		scheduleMsg(&logger, "GOT time %d", timeValue);
	}
}

/**
 * RX handler: call legacy OBD2 CAN hook with a reconstructed CANRxFrame.
 *
 * We keep the OBD2 code unchanged (it still takes CANRxFrame*), but the
 * dispatch decision now lives in the portable CAN core registry.
 */
static void can_obd2_rx_handler(void* user_ctx, const rusefi_can_frame_t* frame) {
	(void)user_ctx;

	CANRxFrame rx;
	memset(&rx, 0, sizeof(rx));

	if ((frame->flags & RUSEFI_CAN_FRAME_FLAG_EXT) != 0) {
		rx.IDE = CAN_IDE_EXT;
		rx.EID = frame->id;
	} else {
		rx.IDE = CAN_IDE_STD;
		rx.SID = frame->id & 0x7FF;
	}

	rx.RTR = ((frame->flags & RUSEFI_CAN_FRAME_FLAG_RTR) != 0) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
	rx.DLC = frame->dlc;
	memcpy(rx.data8, frame->data, 8);

	obdOnCanPacketRx(&rx);
}

/**
 * TX adapter for portable core -> platform transmit
 */
static int can_platform_send(void* user_ctx, const rusefi_can_frame_t* frame) {
	(void)user_ctx;

	CANDriver* device = detectCanDevice(CONFIGB(canRxPin), CONFIGB(canTxPin));
	if (device == NULL) {
		warning(CUSTOM_ERR_CAN_CONFIGURATION, "CAN configuration issue");
		return -1;
	}

	CANTxFrame tx;
	memset(&tx, 0, sizeof(tx));

	if ((frame->flags & RUSEFI_CAN_FRAME_FLAG_EXT) != 0) {
		tx.IDE = CAN_IDE_EXT;
		tx.EID = frame->id;
	} else {
		tx.IDE = CAN_IDE_STD;
		tx.SID = frame->id & 0x7FF;
		/* For compatibility with some existing code patterns, also set EID. */
		tx.EID = frame->id;
	}

	tx.RTR = ((frame->flags & RUSEFI_CAN_FRAME_FLAG_RTR) != 0) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
	tx.DLC = frame->dlc;
	memcpy(tx.data8, frame->data, 8);

	// 1 second timeout
	int result = hal_can_transmit((hal_can_driver_t)device, CAN_ANY_MAILBOX, &tx, 1000);
	if (result == 0) {
		canWriteOk++;
	} else {
		canWriteNotOk++;
	}

	return result;
}

static void can_register_rx_handlers(void) {
	/*
	 * Keep behavior unchanged: we previously logged every received frame and then
	 * called obdOnCanPacketRx (which internally filtered by SID).
	 *
	 * Register both as match-all handlers.
	 */
	rusefi_can_rx_registration_t reg;

	memset(&reg, 0, sizeof(reg));
	reg.id_filter = 0;
	reg.id_mask = 0; /* match-all */
	reg.required_flags = 0;
	reg.handler = can_log_rx_handler;
	reg.user_ctx = NULL;
	(void)rusefi_can_core_register_rx_handler(&s_can_core, &reg);

	memset(&reg, 0, sizeof(reg));
	reg.id_filter = 0;
	reg.id_mask = 0; /* match-all */
	reg.required_flags = 0;
	reg.handler = can_obd2_rx_handler;
	reg.user_ctx = NULL;
	(void)rusefi_can_core_register_rx_handler(&s_can_core, &reg);
}

static void canRead(void) {
	CANDriver* device = detectCanDevice(CONFIGB(canRxPin), CONFIGB(canTxPin));
	if (device == NULL) {
		warning(CUSTOM_ERR_CAN_CONFIGURATION, "CAN configuration issue");
		return;
	}

	int result = hal_can_receive((hal_can_driver_t)device, CAN_ANY_MAILBOX, &rxBuffer, 1000);
	if (result != 0) {
		return;
	}

	canReadCounter++;

	rusefi_can_frame_t frame;
	memset(&frame, 0, sizeof(frame));

	frame.id = rx_id_from_chibios(&rxBuffer);
	frame.flags = rx_flags_from_chibios(&rxBuffer);
	frame.dlc = rxBuffer.DLC;
	memcpy(frame.data, rxBuffer.data8, 8);

	(void)rusefi_can_core_dispatch_rx(&s_can_core, &frame);
}

static void writeStateToCan(void) {
	canInfoNBCBroadcast(engineConfiguration->canNbcType);
}

static msg_t canThread(void* arg) {
	(void)arg;
	chRegSetThreadName("CAN");

	while (true) {
		if (engineConfiguration->canWriteEnabled) {
			writeStateToCan();
		}

		if (engineConfiguration->canReadEnabled) {
			canRead(); // blocking receive, unchanged
		}

		if (engineConfiguration->canSleepPeriod < 10) {
			warning(CUSTOM_OBD_LOW_CAN_PERIOD, "%d too low CAN", engineConfiguration->canSleepPeriod);
			engineConfiguration->canSleepPeriod = 50;
		}

		chThdSleepMilliseconds(engineConfiguration->canSleepPeriod);
	}

#if defined __GNUC__ || defined(__DOXYGEN__)
	return -1;
#endif
}

static void canInfo(void) {
	if (!isCanEnabled) {
		scheduleMsg(&logger, "CAN is not enabled, please enable & restart");
		return;
	}

	scheduleMsg(&logger, "CAN TX %s", hwPortname(CONFIGB(canTxPin)));
	scheduleMsg(&logger, "CAN RX %s", hwPortname(CONFIGB(canRxPin)));
	scheduleMsg(&logger, "type=%d canReadEnabled=%s canWriteEnabled=%s period=%d", engineConfiguration->canNbcType,
			boolToString(engineConfiguration->canReadEnabled),
			boolToString(engineConfiguration->canWriteEnabled),
			engineConfiguration->canSleepPeriod);

	scheduleMsg(&logger, "CAN rx_cnt=%d/tx_ok=%d/tx_not_ok=%d", canReadCounter, canWriteOk, canWriteNotOk);
}

void setCanType(int type) {
	engineConfiguration->canNbcType = (can_nbc_e)type;
	canInfo();
}

void postCanState(TunerStudioOutputChannels* tsOutputChannels) {
	tsOutputChannels->debugIntField1 = isCanEnabled ? canReadCounter : -1;
	tsOutputChannels->debugIntField2 = isCanEnabled ? canWriteOk : -1;
	tsOutputChannels->debugIntField3 = isCanEnabled ? canWriteNotOk : -1;
}

void enableFrankensoCan(DECLARE_ENGINE_PARAMETER_SIGNATURE) {
	CONFIGB(canTxPin) = GPIOB_6;
	CONFIGB(canRxPin) = GPIOB_12;
	engineConfiguration->canReadEnabled = false;
}

void stopCanPins(DECLARE_ENGINE_PARAMETER_SIGNATURE) {
	unmarkPin(activeConfiguration.bc.canTxPin);
	unmarkPin(activeConfiguration.bc.canRxPin);
}

void startCanPins(DECLARE_ENGINE_PARAMETER_SIGNATURE) {
	efiSetPadMode("CAN TX", CONFIGB(canTxPin), PAL_MODE_ALTERNATE(EFI_CAN_TX_AF));
	efiSetPadMode("CAN RX", CONFIGB(canRxPin), PAL_MODE_ALTERNATE(EFI_CAN_RX_AF));
}

void initCan(void) {
	isCanEnabled = (CONFIGB(canTxPin) != GPIO_UNASSIGNED) && (CONFIGB(canRxPin) != GPIO_UNASSIGNED);
	if (isCanEnabled) {
		if (!isValidCanTxPin(CONFIGB(canTxPin))) {
			firmwareError(CUSTOM_OBD_70, "invalid CAN TX %s", hwPortname(CONFIGB(canTxPin)));
		}
		if (!isValidCanRxPin(CONFIGB(canRxPin))) {
			firmwareError(CUSTOM_OBD_70, "invalid CAN RX %s", hwPortname(CONFIGB(canRxPin)));
		}
	}

	addConsoleAction("caninfo", canInfo);
	if (!isCanEnabled) {
		return;
	}

#if STM32_CAN_USE_CAN2 || defined(__DOXYGEN__)
	// CAN1 is required for CAN2
	hal_can_start(hal_can_get_driver(HAL_CAN_DRIVER_1), &canConfig500);
	hal_can_start(hal_can_get_driver(HAL_CAN_DRIVER_2), &canConfig500);
#else
	hal_can_start(hal_can_get_driver(HAL_CAN_DRIVER_1), &canConfig500);
#endif /* STM32_CAN_USE_CAN2 */

	/* Initialize core and wire TX/RX dispatch */
	rusefi_can_core_init(&s_can_core);

	rusefi_can_tx_iface_t tx_iface;
	tx_iface.send = can_platform_send;
	tx_iface.user_ctx = NULL;
	rusefi_can_core_set_tx_iface(&s_can_core, &tx_iface);

	can_register_rx_handlers();

	chThdCreateStatic(canTreadStack, sizeof(canTreadStack), NORMALPRIO, (tfunc_t)(void*)canThread, NULL);

	startCanPins();
}

#endif /* EFI_CAN_SUPPORT */
