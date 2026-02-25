#include "rusefi_headless_sensors.h"

#include <stdlib.h>

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
