#include "global.h"

extern "C" {
#include "firmware/headless/esm/rusefi_headless_esm.h"
}

namespace {

struct FakeTime {
	uint64_t now = 0;
};

static uint64_t fake_time_now(void* user_ctx) {
	auto* t = reinterpret_cast<FakeTime*>(user_ctx);
	return t->now;
}

}  // namespace

TEST(headless_esm, init_sets_stopped_and_not_spinning) {
	rusefi_esm_ctx_t ctx{};
	ctx.state = RUSEFI_ESM_RUNNING;
	ctx.is_spinning = true;
	ctx.last_fast_tick_time_nt = 123;
	ctx.last_slow_tick_time_nt = 456;

	rusefi_esm_init(&ctx);

	ASSERT_EQ(RUSEFI_ESM_STOPPED, rusefi_esm_get_state(&ctx));
	ASSERT_FALSE(ctx.is_spinning);
	ASSERT_EQ(0u, ctx.last_fast_tick_time_nt);
	ASSERT_EQ(0u, ctx.last_slow_tick_time_nt);
}

TEST(headless_esm, on_rpm_transitions_stopped_to_cranking_to_running_and_running_is_sticky_until_rpm_zero) {
	rusefi_esm_ctx_t ctx{};
	rusefi_esm_init(&ctx);

	const int crankingThreshold = 400;

	// rpm>0 but below threshold => STOPPED/SPINNING_UP -> CRANKING
	ASSERT_TRUE(rusefi_esm_on_rpm(&ctx, 200, crankingThreshold));
	ASSERT_EQ(RUSEFI_ESM_CRANKING, rusefi_esm_get_state(&ctx));
	ASSERT_TRUE(rusefi_esm_is_cranking(&ctx, 200));
	ASSERT_FALSE(rusefi_esm_is_running(&ctx));

	// Above threshold => RUNNING
	ASSERT_TRUE(rusefi_esm_on_rpm(&ctx, 500, crankingThreshold));
	ASSERT_EQ(RUSEFI_ESM_RUNNING, rusefi_esm_get_state(&ctx));
	ASSERT_TRUE(rusefi_esm_is_running(&ctx));

	// Drop below threshold should stay RUNNING (hysteresis/sticky)
	ASSERT_FALSE(rusefi_esm_on_rpm(&ctx, 200, crankingThreshold));
	ASSERT_EQ(RUSEFI_ESM_RUNNING, rusefi_esm_get_state(&ctx));

	// rpm==0 always forces STOPPED
	ASSERT_TRUE(rusefi_esm_on_rpm(&ctx, 0, crankingThreshold));
	ASSERT_EQ(RUSEFI_ESM_STOPPED, rusefi_esm_get_state(&ctx));
	ASSERT_TRUE(rusefi_esm_is_stopped(&ctx, 0));
}

TEST(headless_esm, request_spinning_up_only_enters_from_fully_stopped_and_non_spinning_when_enabled) {
	rusefi_esm_ctx_t ctx{};
	rusefi_esm_init(&ctx);

	// Disabled feature: should not change state, should return whether already spinning up (false here)
	ASSERT_FALSE(rusefi_esm_request_spinning_up(&ctx, false));
	ASSERT_EQ(RUSEFI_ESM_STOPPED, rusefi_esm_get_state(&ctx));

	// Enabled: should enter spinning up
	ASSERT_TRUE(rusefi_esm_request_spinning_up(&ctx, true));
	ASSERT_EQ(RUSEFI_ESM_SPINNING_UP, rusefi_esm_get_state(&ctx));
	ASSERT_TRUE(ctx.is_spinning);
	ASSERT_TRUE(rusefi_esm_is_spinning_up(&ctx));

	// Calling again should remain in spinning up (idempotent)
	ASSERT_TRUE(rusefi_esm_request_spinning_up(&ctx, true));
	ASSERT_EQ(RUSEFI_ESM_SPINNING_UP, rusefi_esm_get_state(&ctx));

	// If already spinning (is_spinning=true), but state stopped, should NOT enter spinning_up (gate condition)
	rusefi_esm_on_stop_spinning(&ctx);
	ASSERT_EQ(RUSEFI_ESM_STOPPED, rusefi_esm_get_state(&ctx));
	ASSERT_FALSE(ctx.is_spinning);

	// Simulate spinning observed without being in spinning_up (force)
	ctx.is_spinning = true;
	ASSERT_FALSE(rusefi_esm_request_spinning_up(&ctx, true));
	ASSERT_EQ(RUSEFI_ESM_STOPPED, rusefi_esm_get_state(&ctx));
}

TEST(headless_esm, spinning_up_predicates_match_contract) {
	rusefi_esm_ctx_t ctx{};
	rusefi_esm_init(&ctx);

	// STOPPED + rpm=0 => stopped
	ASSERT_TRUE(rusefi_esm_is_stopped(&ctx, 0));
	ASSERT_FALSE(rusefi_esm_is_cranking(&ctx, 0));

	// Enter spinning up
	ASSERT_TRUE(rusefi_esm_request_spinning_up(&ctx, true));

	// In SPINNING_UP:
	// - rpm==0 should be considered stopped
	ASSERT_TRUE(rusefi_esm_is_stopped(&ctx, 0));
	// - rpm>0 should be considered cranking (legacy behavior)
	ASSERT_TRUE(rusefi_esm_is_cranking(&ctx, 100));
}

TEST(headless_esm, tick_records_time_and_validates_deps_contract) {
	rusefi_esm_ctx_t ctx{};
	rusefi_esm_init(&ctx);

	FakeTime t{};
	rusefi_esm_deps_t deps{};
	deps.user_ctx = &t;
	deps.get_time_now_nt = &fake_time_now;
	deps.get_sensors_snapshot = nullptr;

	// Invalid deps -> -1
	ASSERT_NE(0, rusefi_esm_fast_tick(&ctx, nullptr));
	ASSERT_NE(0, rusefi_esm_slow_tick(&ctx, nullptr));

	t.now = 100;
	ASSERT_EQ(0, rusefi_esm_fast_tick(&ctx, &deps));
	ASSERT_EQ(100u, ctx.last_fast_tick_time_nt);

	t.now = 200;
	ASSERT_EQ(0, rusefi_esm_slow_tick(&ctx, &deps));
	ASSERT_EQ(200u, ctx.last_slow_tick_time_nt);
}
