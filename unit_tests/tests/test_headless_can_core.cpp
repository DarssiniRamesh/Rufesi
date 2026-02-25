#include "global.h"

extern "C" {
#include "firmware/headless/can/rusefi_can_core.h"
}

#include <cstring>

namespace {
struct RxCapture {
	int called = 0;
	rusefi_can_frame_t last{};
};

static void rx_handler_capture(void* user_ctx, const rusefi_can_frame_t* frame) {
	auto* cap = reinterpret_cast<RxCapture*>(user_ctx);
	cap->called++;
	cap->last = *frame;
}

struct TxCapture {
	int called = 0;
	int return_code = 7;
	rusefi_can_frame_t last{};
};

static int tx_send_capture(void* user_ctx, const rusefi_can_frame_t* frame) {
	auto* cap = reinterpret_cast<TxCapture*>(user_ctx);
	cap->called++;
	cap->last = *frame;
	return cap->return_code;
}
}  // namespace

TEST(headless_can_core, dispatch_rx_matches_id_and_required_flags) {
	rusefi_can_core_t coreStorage;
	rusefi_can_core_t* core = &coreStorage;

	rusefi_can_core_init(core);

	RxCapture capMatch{};
	RxCapture capNoMatch{};

	// Match exact 11-bit ID 0x123, require EXT flag not set, RTR not required.
	rusefi_can_rx_registration_t regMatch{};
	regMatch.user_ctx = &capMatch;
	regMatch.id_filter = 0x123;
	regMatch.id_mask = 0x7FF;
	regMatch.required_flags = 0;
	regMatch.handler = &rx_handler_capture;
	ASSERT_EQ(0, rusefi_can_core_register_rx_handler(core, &regMatch));

	// Different ID, should not fire.
	rusefi_can_rx_registration_t regNoMatch{};
	regNoMatch.user_ctx = &capNoMatch;
	regNoMatch.id_filter = 0x124;
	regNoMatch.id_mask = 0x7FF;
	regNoMatch.required_flags = 0;
	regNoMatch.handler = &rx_handler_capture;
	ASSERT_EQ(0, rusefi_can_core_register_rx_handler(core, &regNoMatch));

	rusefi_can_frame_t frame{};
	frame.id = 0x123;
	frame.dlc = 2;
	frame.flags = 0;
	frame.data[0] = 0xAA;
	frame.data[1] = 0x55;

	const int invoked = rusefi_can_core_dispatch_rx(core, &frame);
	ASSERT_EQ(1, invoked);
	ASSERT_EQ(1, capMatch.called);
	ASSERT_EQ(0, capNoMatch.called);

	ASSERT_EQ(frame.id, capMatch.last.id);
	ASSERT_EQ(frame.dlc, capMatch.last.dlc);
	ASSERT_EQ(frame.flags, capMatch.last.flags);
	ASSERT_EQ(0, std::memcmp(frame.data, capMatch.last.data, 8));
}

TEST(headless_can_core, dispatch_rx_respects_required_flags) {
	rusefi_can_core_t coreStorage;
	rusefi_can_core_t* core = &coreStorage;
	rusefi_can_core_init(core);

	RxCapture capExtOnly{};

	rusefi_can_rx_registration_t reg{};
	reg.user_ctx = &capExtOnly;
	reg.id_filter = 0x1ABCDEu;
	reg.id_mask = 0x1FFFFFFFu;
	reg.required_flags = RUSEFI_CAN_FRAME_FLAG_EXT;
	reg.handler = &rx_handler_capture;
	ASSERT_EQ(0, rusefi_can_core_register_rx_handler(core, &reg));

	rusefi_can_frame_t frameNoExt{};
	frameNoExt.id = 0x1ABCDEu;
	frameNoExt.flags = 0;

	ASSERT_EQ(0, rusefi_can_core_dispatch_rx(core, &frameNoExt));
	ASSERT_EQ(0, capExtOnly.called);

	rusefi_can_frame_t frameExt{};
	frameExt.id = 0x1ABCDEu;
	frameExt.flags = RUSEFI_CAN_FRAME_FLAG_EXT;
	frameExt.dlc = 1;
	frameExt.data[0] = 0x42;

	ASSERT_EQ(1, rusefi_can_core_dispatch_rx(core, &frameExt));
	ASSERT_EQ(1, capExtOnly.called);
	ASSERT_EQ(0x42, capExtOnly.last.data[0]);
}

TEST(headless_can_core, send_returns_error_when_tx_not_configured) {
	rusefi_can_core_t coreStorage;
	rusefi_can_core_t* core = &coreStorage;
	rusefi_can_core_init(core);

	rusefi_can_frame_t frame{};
	frame.id = 0x100;
	frame.dlc = 0;
	frame.flags = 0;

	// Contract: -2 when tx interface missing.
	ASSERT_EQ(-2, rusefi_can_core_send(core, &frame));

	// Also validate NULL inputs are handled.
	ASSERT_EQ(-1, rusefi_can_core_send(nullptr, &frame));
	ASSERT_EQ(-1, rusefi_can_core_send(core, nullptr));
}

TEST(headless_can_core, send_calls_tx_callback_and_propagates_return_code) {
	rusefi_can_core_t coreStorage;
	rusefi_can_core_t* core = &coreStorage;
	rusefi_can_core_init(core);

	TxCapture txCap{};
	txCap.return_code = 123;

	rusefi_can_tx_iface_t tx{};
	tx.user_ctx = &txCap;
	tx.send = &tx_send_capture;
	rusefi_can_core_set_tx_iface(core, &tx);

	rusefi_can_frame_t frame{};
	frame.id = 0x321;
	frame.flags = RUSEFI_CAN_FRAME_FLAG_RTR;
	frame.dlc = 3;
	frame.data[0] = 1;
	frame.data[1] = 2;
	frame.data[2] = 3;

	ASSERT_EQ(123, rusefi_can_core_send(core, &frame));
	ASSERT_EQ(1, txCap.called);
	ASSERT_EQ(frame.id, txCap.last.id);
	ASSERT_EQ(frame.flags, txCap.last.flags);
	ASSERT_EQ(frame.dlc, txCap.last.dlc);
	ASSERT_EQ(0, std::memcmp(frame.data, txCap.last.data, 8));
}
