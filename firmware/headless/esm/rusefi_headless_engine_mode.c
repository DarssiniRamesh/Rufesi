#include "rusefi_headless_engine_mode.h"

// PUBLIC_INTERFACE
int rusefi_headless_pack_engine_mode(int fuel_algorithm, int injection_mode, int ignition_mode) {
	/**
	 * Pack mode bits in a stable format used by legacy console and TS output.
	 *
	 * Bit layout (legacy):
	 *  - high nibble: fuel algorithm
	 *  - next 2 bits: injection mode
	 *  - low 2 bits: ignition mode
	 */
	return (fuel_algorithm << 4) | (injection_mode << 2) | ignition_mode;
}
