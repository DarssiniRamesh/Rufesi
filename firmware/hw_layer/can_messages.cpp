/**
 * @file can_messages.cpp
 * @brief CAN message encoding and high-level packet composition.
 *
 * This file is platform-agnostic with respect to CAN IO: it does not call ChibiOS
 * canTransmit/canReceive or touch CANDriver lifecycle. Instead, it sends frames
 * via the portable rusefi_can_core API configured by the platform layer.
 *
 * NOTE: We intentionally keep the legacy txmsg/commonTxInit/sendCanMessage API
 * intact to avoid changing existing behavior/users (e.g. OBD2 implementation).
 */

#include "global.h"

#if EFI_CAN_SUPPORT || defined(__DOXYGEN__)

#include "engine_configuration.h"
#include "can_hw.h"
#include "engine_state.h"
#include "vehicle_speed.h"

#include "headless/can/rusefi_can_core.h"

EXTERN_ENGINE
;

/**
 * Legacy global TX buffer used by existing code (e.g. obd2.cpp).
 * Kept to preserve behavior and avoid a wide refactor in one step.
 */
CANTxFrame txmsg;

static void setShortValue(CANTxFrame* msg, int value, int offset) {
	msg->data8[offset] = value;
	msg->data8[offset + 1] = value >> 8;
}

void setTxBit(int offset, int index) {
	txmsg.data8[offset] = txmsg.data8[offset] | (1 << index);
}

void commonTxInit(int eid) {
	memset(&txmsg, 0, sizeof(txmsg));
	txmsg.IDE = CAN_IDE_STD;

	/*
	 * ChibiOS uses SID for standard identifiers and EID for extended.
	 * The old code wrote EID even for STD frames; to preserve existing
	 * behavior while making intent explicit, we populate both.
	 */
	txmsg.SID = eid & 0x7FF;
	txmsg.EID = eid;

	txmsg.RTR = CAN_RTR_DATA;
	txmsg.DLC = 8;
}

static void sendCanMessage2(int size) {
	rusefi_can_core_t* core = canGetCore();
	if (core == NULL) {
		/* Platform layer not initialized. Preserve "no send" behavior. */
		return;
	}

	rusefi_can_frame_t frame;
	memset(&frame, 0, sizeof(frame));

	txmsg.DLC = size;

	/* Convert legacy txmsg to portable frame */
	frame.dlc = (uint8_t)size;
	if (txmsg.IDE == CAN_IDE_EXT) {
		frame.flags |= RUSEFI_CAN_FRAME_FLAG_EXT;
		frame.id = (uint32_t)txmsg.EID;
	} else {
		frame.id = (uint32_t)txmsg.SID;
	}

	if (txmsg.RTR == CAN_RTR_REMOTE) {
		frame.flags |= RUSEFI_CAN_FRAME_FLAG_RTR;
	}

	memcpy(frame.data, txmsg.data8, 8);

	/* TX is performed by the platform-provided tx iface */
	(void)rusefi_can_core_send(core, &frame);
}

void sendCanMessage() {
	sendCanMessage2(8);
}

static void canDashboardBMW(void) {
	// BMW Dashboard
	commonTxInit(CAN_BMW_E46_SPEED);
	setShortValue(&txmsg, 10 * 8, 1);
	sendCanMessage();

	commonTxInit(CAN_BMW_E46_RPM);
	setShortValue(&txmsg, (int)(GET_RPM() * 6.4), 2);
	sendCanMessage();

	commonTxInit(CAN_BMW_E46_DME2);
	setShortValue(&txmsg, (int)((engine->sensors.clt + 48.373) / 0.75), 1);
	sendCanMessage();
}

static void canMazdaRX8(void) {
	commonTxInit(CAN_MAZDA_RX_STEERING_WARNING);
	// todo: something needs to be set here? see http://rusefi.com/wiki/index.php?title=Vehicle:Mazda_Rx8_2004
	sendCanMessage();

	commonTxInit(CAN_MAZDA_RX_RPM_SPEED);

	float kph = getVehicleSpeed();

	setShortValue(&txmsg, SWAP_UINT16(GET_RPM() * 4), 0);
	setShortValue(&txmsg, 0xFFFF, 2);
	setShortValue(&txmsg, SWAP_UINT16((int)(100 * kph + 10000)), 4);
	setShortValue(&txmsg, 0, 6);
	sendCanMessage();

	commonTxInit(CAN_MAZDA_RX_STATUS_1);
	txmsg.data8[0] = 0xFE; // Unknown
	txmsg.data8[1] = 0xFE; // Unknown
	txmsg.data8[2] = 0xFE; // Unknown
	txmsg.data8[3] = 0x34; // DSC OFF in combo with byte 5 Live data only seen 0x34
	txmsg.data8[4] = 0x00; // B01000000; // Brake warning B00001000;  //ABS warning
	txmsg.data8[5] = 0x40; // TCS in combo with byte 3
	txmsg.data8[6] = 0x00; // Unknown
	txmsg.data8[7] = 0x00; // Unused
	sendCanMessage();

	commonTxInit(CAN_MAZDA_RX_STATUS_2);
	txmsg.data8[0] = (uint8_t)(engine->sensors.clt + 69); // temp gauge
	txmsg.data8[1] = ((int16_t)(engine->engineState.vssEventCounter * (engineConfiguration->vehicleSpeedCoef * 0.277 * 2.58))) & 0xff;
	txmsg.data8[2] = 0x00; // unknown
	txmsg.data8[3] = 0x00; // unknown
	txmsg.data8[4] = 0x01; // Oil Pressure (not really a gauge)
	txmsg.data8[5] = 0x00; // check engine light
	txmsg.data8[6] = 0x00; // Coolant, oil and battery
	if ((GET_RPM() > 0) && (engine->sensors.vBatt < 13)) {
		setTxBit(6, 6); // battery light
	}
	if (engine->sensors.clt > 105) {
		setTxBit(6, 1); // coolant light
	}
	// oil pressure warning lamp bit is 7
	txmsg.data8[7] = 0x00; // unused
	sendCanMessage();
}

static void canDashboardFiat(void) {
	// Fiat Dashboard
	commonTxInit(CAN_FIAT_MOTOR_INFO);
	setShortValue(&txmsg, (int)(engine->sensors.clt - 40), 3); // Coolant Temp
	setShortValue(&txmsg, GET_RPM() / 32, 6); // RPM
	sendCanMessage();
}

static void canDashboardVAG(void) {
	// VAG Dashboard
	commonTxInit(CAN_VAG_RPM);
	setShortValue(&txmsg, GET_RPM() * 4, 2); // RPM
	sendCanMessage();

	commonTxInit(CAN_VAG_CLT);
	setShortValue(&txmsg, (int)((engine->sensors.clt + 48.373) / 0.75), 1); // Coolant Temp
	sendCanMessage();
}

/* Exported for can_hw.cpp thread loop. */
void canInfoNBCBroadcast(can_nbc_e typeOfNBC) {
	switch (typeOfNBC) {
	case CAN_BUS_NBC_BMW:
		canDashboardBMW();
		break;
	case CAN_BUS_NBC_FIAT:
		canDashboardFiat();
		break;
	case CAN_BUS_NBC_VAG:
		canDashboardVAG();
		break;
	case CAN_BUS_MAZDA_RX8:
		canMazdaRX8();
		break;
	default:
		break;
	}
}

#endif /* EFI_CAN_SUPPORT */
