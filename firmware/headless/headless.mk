# Headless module build fragment.
# This intentionally introduces new compilation units only (no behavior changes).
# It can be included by firmware, unit tests, and simulator makefiles.

HEADLESS_SRC = \
	$(PROJECT_DIR)/headless/sensors/rusefi_headless_sensors.c \
	$(PROJECT_DIR)/headless/sensors/rusefi_headless_sensors_snapshot.c \
	$(PROJECT_DIR)/headless/esm/rusefi_headless_esm.c \
	$(PROJECT_DIR)/headless/esm/rusefi_headless_engine_mode.c \
	$(PROJECT_DIR)/headless/can/rusefi_can_core.c

HEADLESS_INCDIR = \
	$(PROJECT_DIR)/headless \
	$(PROJECT_DIR)/headless/sensors \
	$(PROJECT_DIR)/headless/esm \
	$(PROJECT_DIR)/headless/can
