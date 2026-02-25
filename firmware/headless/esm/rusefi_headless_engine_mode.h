#ifndef RUSEFI_HEADLESS_ENGINE_MODE_H
#define RUSEFI_HEADLESS_ENGINE_MODE_H

/**
 * @file rusefi_headless_engine_mode.h
 * @brief Headless/portable helpers for packing engine “mode” values.
 *
 * This module exists to migrate legacy “embedded engine-mode logic” (bit packing)
 * out of platform/console code into a small portable API with an explicit contract.
 *
 * The resulting packed value is used by:
 *  - dev console text status logging
 *  - TunerStudio output channels (engineMode field)
 *
 * Contract:
 *  Inputs:
 *    - fuel_algorithm, injection_mode, ignition_mode: expected to be small non-negative ints
 *      matching firmware enums (engine_load_mode_e / injection_mode_e / ignition_mode_e).
 *  Output:
 *    - Packed int: (fuel_algorithm << 4) | (injection_mode << 2) | ignition_mode
 *  Invariants:
 *    - No side effects, no logging, no firmware globals. Pure function.
 *  Errors:
 *    - None (callers are responsible for providing sane enum values).
 */

#ifdef __cplusplus
extern "C" {
#endif

// PUBLIC_INTERFACE
int rusefi_headless_pack_engine_mode(int fuel_algorithm, int injection_mode, int ignition_mode);

#ifdef __cplusplus
}
#endif

#endif /* RUSEFI_HEADLESS_ENGINE_MODE_H */
