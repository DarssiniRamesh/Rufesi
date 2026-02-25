/*
 * @file obd2.cpp
 *
 * ISO 15765-4
 * http://en.wikipedia.org/wiki/OBD-II_PIDs
 *
 * @date Jun 9, 2015
 * @author Andrey Belomutskiy, (c) 2012-2018
 *
 * This file is part of rusEfi - see http://rusefi.com
 *
 * rusEfi is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * rusEfi is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "global.h"

#if EFI_CAN_SUPPORT || defined(__DOXYGEN__)


#include "engine.h"
#include "obd2.h"
#include "can_hw.h"
#include "vehicle_speed.h"
#include "map.h"
#include "maf.h"
#include "tps.h"
#include "engine_math.h"
#include "fuel_math.h"

#include "headless/can/rusefi_can_core.h"

EXTERN_ENGINE
;

/**
 * Send an OBD2 response frame using the portable CAN core TX interface.
 *
 * Contract:
 *  - Inputs:
 *      - sid: standard 11-bit CAN identifier (we use STD frames for OBD2)
 *      - payload: pointer to exactly 8 bytes (CAN classic)
 *  - Side effects:
 *      - Transmits a CAN frame via the platform-configured TX interface (can_hw.cpp)
 *  - Errors:
 *      - If CAN is not initialized, the frame is dropped (matches legacy "no send" behavior).
 */
static void obd_send_can_frame(uint32_t sid, const uint8_t payload[8]) {
	rusefi_can_core_t* core = canGetCore();
	if (core == NULL) {
		// Platform layer not initialized/enabled. Preserve legacy "no send" behavior.
		return;
	}

	rusefi_can_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	frame.id = sid & 0x7FF;
	frame.dlc = 8;
	memcpy(frame.data, payload, 8);

	(void)rusefi_can_core_send(core, &frame);
}

static LoggingWithStorage logger("obd2");

static const int16_t supportedPids0120[] = { 
	PID_MONITOR_STATUS,
	PID_FUEL_SYSTEM_STATUS,
	PID_ENGINE_LOAD,
	PID_COOLANT_TEMP,
	PID_INTAKE_MAP,
	PID_RPM,
	PID_SPEED,
	PID_TIMING_ADVANCE,
	PID_INTAKE_TEMP,
	PID_THROTTLE,
	-1
};

static const int16_t supportedPids2140[] = {
	-1
};

static const int16_t supportedPids4160[] = { 
	PID_FUEL_RATE,
	-1
};

static void obdSendPacket(int mode, int PID, int numBytes, uint32_t iValue) {
	uint8_t payload[8];
	memset(payload, 0, sizeof(payload));

	// write number of bytes
	payload[0] = (uint8_t)(2 + numBytes);
	// write 2 bytes of header
	payload[1] = (uint8_t)(0x40 + mode);
	payload[2] = (uint8_t)PID;
	// write N data bytes
	for (int i = 8 * (numBytes - 1), j = 3; i >= 0; i -= 8, j++) {
		payload[j] = (uint8_t)((iValue >> i) & 0xff);
	}

	obd_send_can_frame(OBD_TEST_RESPONSE, payload);
}

static void obdSendValue(int mode, int PID, int numBytes, float value) {
	efiAssertVoid(CUSTOM_ERR_6662, numBytes <= 2, "invalid numBytes");
	int iValue = (int)efiRound(value, 1.0f);
	// clamp to uint8_t (0..255) or uint16_t (0..65535)
	iValue = maxI(minI(iValue, (numBytes == 1) ? 255 : 65535), 0);
	obdSendPacket(mode, PID, numBytes, iValue);
}


//#define MOCK_SUPPORTED_PIDS 0xffffffff

static void obdWriteSupportedPids(int PID, int bitOffset, const int16_t *supportedPids) {
	uint32_t value = 0;
	// gather all 32 bit fields
	for (int i = 0; i < 32 && supportedPids[i] > 0; i++)
		value |= 1 << (31 + bitOffset - supportedPids[i]);

#ifdef MOCK_SUPPORTED_PIDS
	// for OBD debug
	value = MOCK_SUPPORTED_PIDS;
#endif

	scheduleMsg(&logger, "Write bitfields 0x%08x", value);
	obdSendPacket(1, PID, 4, value);
}

static void handleGetDataRequest(CANRxFrame *rx) {
	int pid = rx->data8[2];
	switch (pid) {
	case PID_SUPPORTED_PIDS_REQUEST_01_20:
		scheduleMsg(&logger, "Got lookup request 01-20");
		obdWriteSupportedPids(pid, 1, supportedPids0120);
		break;
	case PID_SUPPORTED_PIDS_REQUEST_21_40:
		scheduleMsg(&logger, "Got lookup request 21-40");
		obdWriteSupportedPids(pid, 21, supportedPids2140);
		break;
	case PID_SUPPORTED_PIDS_REQUEST_41_60:
		scheduleMsg(&logger, "Got lookup request 41-60");
		obdWriteSupportedPids(pid, 41, supportedPids4160);
		break;
	case PID_MONITOR_STATUS:
		scheduleMsg(&logger, "Got monitor status request");
		obdSendPacket(1, pid, 4, 0);	// todo: add statuses
		break;
	case PID_FUEL_SYSTEM_STATUS:
		scheduleMsg(&logger, "Got fuel system status request");
		// todo: add statuses
		obdSendValue(1, pid, 2, (2<<8)|(0));	// 2 = "Closed loop, using oxygen sensor feedback to determine fuel mix"
		break;
	case PID_ENGINE_LOAD:
		scheduleMsg(&logger, "Got engine load request");
		obdSendValue(1, pid, 1, getEngineLoadT(PASS_ENGINE_PARAMETER_SIGNATURE) * 2.55f);
		break;
	case PID_COOLANT_TEMP:
		scheduleMsg(&logger, "Got CLT request");
		obdSendValue(1, pid, 1, engine->sensors.clt + 40.0f);
		break;
	case PID_INTAKE_MAP:
		scheduleMsg(&logger, "Got MAP request");
		obdSendValue(1, pid, 1, getMap());
		break;
	case PID_RPM:
		scheduleMsg(&logger, "Got RPM request");
		obdSendValue(1, pid, 2, GET_RPM() * 4.0f);	//	rotation/min.	(A*256+B)/4
		break;
	case PID_SPEED:
		scheduleMsg(&logger, "Got speed request");
		obdSendValue(1, pid, 1, getVehicleSpeed());
		break;
	case PID_TIMING_ADVANCE: {
		scheduleMsg(&logger, "Got timing request");
		float timing = engine->engineState.timingAdvance;
		timing = (timing > 360.0f) ? (timing - 720.0f) : timing;
		obdSendValue(1, pid, 1, (timing + 64.0f) * 2.0f);		// angle before TDC.	(A/2)-64
		break;
		}
	case PID_INTAKE_TEMP:
		scheduleMsg(&logger, "Got IAT request");
		obdSendValue(1, pid, 1, engine->sensors.iat + 40.0f);
		break;
	case PID_INTAKE_MAF:
		scheduleMsg(&logger, "Got MAF request");
		obdSendValue(1, pid, 2, getMaf(PASS_ENGINE_PARAMETER_SIGNATURE) * 100.0f);	// grams/sec	(A*256+B)/100
		break;
	case PID_THROTTLE:
		scheduleMsg(&logger, "Got throttle request");
		obdSendValue(1, pid, 1, getTPS(PASS_ENGINE_PARAMETER_SIGNATURE) * 2.55f);	// (A*100/255)
		break;
	case PID_FUEL_RATE:
		scheduleMsg(&logger, "Got fuel rate request");
		obdSendValue(1, pid, 2, engine->engineState.fuelConsumption.perSecondConsumption * 20.0f);	//	L/h.	(A*256+B)/20
		break;
	default:
		scheduleMsg(&logger, "Got unhandled request (PID 0x%02x)", pid);
	}
	
}

static void handleDtcRequest(int numCodes, int *dtcCode, uint8_t payload[8]) {
	int numBytes = numCodes * 2;

	// write CAN-TP Single Frame header?
	payload[0] = (uint8_t)((0 << 4) | numBytes);
	for (int i = 0, j = 1; i < numCodes; i++) {
		payload[j++] = (uint8_t)((dtcCode[i] >> 8) & 0xff);
		payload[j++] = (uint8_t)(dtcCode[i] & 0xff);
	}
}

#if EFI_CAN_SUPPORT || defined(__DOXYGEN__)
// PUBLIC_INTERFACE
void obdOnCanFrameRx(const rusefi_can_frame_t* frame) {
	/**
	 * Portable CAN-core RX handler for OBD2.
	 *
	 * Contract:
	 *  - Only standard frames with SID==OBD_TEST_REQUEST are handled (legacy behavior).
	 *  - Payload is treated as 8 bytes (classic CAN); if DLC < 3, we ignore (safe guard).
	 */
	if (frame == NULL) {
		return;
	}

	/* Legacy behavior only handled standard-ID OBD frames */
	if ((frame->flags & RUSEFI_CAN_FRAME_FLAG_EXT) != 0) {
		return;
	}

	const uint32_t sid = frame->id & 0x7FF;
	if (sid != OBD_TEST_REQUEST) {
		return;
	}

	if (frame->dlc < 2) {
		return;
	}

	/* Keep existing decode logic by using a stack CANRxFrame as an internal structure. */
	CANRxFrame rx;
	memset(&rx, 0, sizeof(rx));
	rx.IDE = CAN_IDE_STD;
	rx.SID = (uint16_t)sid;
	rx.DLC = frame->dlc;
	memcpy(rx.data8, frame->data, 8);

	if (rx.data8[0] == 2 && rx.data8[1] == OBD_CURRENT_DATA) {
		handleGetDataRequest(&rx);
	} else if (rx.data8[0] == 1 && rx.data8[1] == OBD_STORED_DIAGNOSTIC_TROUBLE_CODES) {
		scheduleMsg(&logger, "Got stored DTC request");

		uint8_t payload[8];
		memset(payload, 0, sizeof(payload));
		// todo: implement stored/pending difference?
		handleDtcRequest(1, &engine->engineState.warnings.lastErrorCode, payload);
		obd_send_can_frame(OBD_TEST_RESPONSE, payload);
	} else if (rx.data8[0] == 1 && rx.data8[1] == OBD_PENDING_DIAGNOSTIC_TROUBLE_CODES) {
		scheduleMsg(&logger, "Got pending DTC request");

		uint8_t payload[8];
		memset(payload, 0, sizeof(payload));
		// todo: implement stored/pending difference?
		handleDtcRequest(1, &engine->engineState.warnings.lastErrorCode, payload);
		obd_send_can_frame(OBD_TEST_RESPONSE, payload);
	} else {
		scheduleMsg(&logger, "Got unhandled OBD message");
	}
}
#endif /* EFI_CAN_SUPPORT */

#if HAL_USE_CAN || defined(__DOXYGEN__)
// PUBLIC_INTERFACE
void obdOnCanPacketRx(CANRxFrame *rx) {
	/**
	 * Legacy shim (CANRxFrame-based) -> portable frame handler.
	 *
	 * This function should be considered deprecated for new code. The canonical
	 * entrypoint is obdOnCanFrameRx().
	 */
	if (rx == NULL) {
		return;
	}

	rusefi_can_frame_t frame;
	memset(&frame, 0, sizeof(frame));

	frame.dlc = rx->DLC;
	memcpy(frame.data, rx->data8, 8);

	if (rx->IDE == CAN_IDE_EXT) {
		frame.flags |= RUSEFI_CAN_FRAME_FLAG_EXT;
		frame.id = rx->EID;
	} else {
		frame.id = rx->SID;
	}

	if (rx->RTR == CAN_RTR_REMOTE) {
		frame.flags |= RUSEFI_CAN_FRAME_FLAG_RTR;
	}

	obdOnCanFrameRx(&frame);
}
#endif /* HAL_USE_CAN */

#endif /* EFI_CAN_SUPPORT */
