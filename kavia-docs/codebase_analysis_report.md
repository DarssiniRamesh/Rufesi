# Rufesi (rusEFI) Codebase Analysis Report

## Overview

This report documents issues, gaps, and potential build/test/runtime errors identified
through static analysis of the Rufesi monorepo, which contains the rusEFI STM32 ECU
firmware, Java console tools, simulator, unit tests, and KiCAD hardware designs.

---

## Critical Issues

### 1. Empty Git Submodules (Build Blocker — All Targets)

**Affected paths:**
- `firmware/ChibiOS4/` (ChibiOS RTOS — empty)
- `firmware/ChibiOS-Contrib/` (ChibiOS Contrib HAL — empty)
- `unit_tests/googletest/` (Google Test framework — empty)
- `hardware/rusefi_lib/` (KiCAD shared libraries — empty)

**Impact:**
- **Firmware build** (`firmware/Makefile`): Fails immediately. The Makefile includes
  `ChibiOS4/os/common/startup/ARMCMx/compilers/GCC/mk/...`, HAL, RT kernel, and
  port makefiles from ChibiOS4 and ChibiOS-Contrib. None of these files exist.
  `firmware/rusefi.mk` detects this and prints `$(error Please run 'make' again)`
  but the auto-`git submodule update --init` also fails if there's no `.git` history.
- **Simulator build** (`simulator/Makefile`): Same ChibiOS dependency; fails identically.
- **Unit test build** (`unit_tests/Makefile`): `gtest-all.cpp` and `gmock-all.cpp`
  `#include` source files from `googletest/googletest/src/` and
  `googletest/googlemock/src/` which don't exist. Compilation fails.
- **Hardware designs**: KiCAD projects referencing `rusefi_lib` footprints/symbols
  will have unresolved library references.

**Fix:** Run `git submodule update --init` from a proper git clone, or populate the
submodule directories with the correct versions:
- ChibiOS: `stable_17.6.rusefi` branch from `https://github.com/rusefi/ChibiOS.git`
- ChibiOS-Contrib: from `https://github.com/ChibiOS/ChibiOS-Contrib.git`
- googletest: `v1.8.x` branch from `https://github.com/google/googletest.git`
- rusefi_lib: from `https://github.com/rusefi/kicad-libraries`

---

### 2. Unit Test Include Path Errors for Headless Module Tests

**Affected files:**
- `unit_tests/tests/test_headless_can_core.cpp`
- `unit_tests/tests/test_headless_sensors_snapshot.cpp`
- `unit_tests/tests/test_headless_esm.cpp`

**Problem:**
These test files use include paths prefixed with `firmware/`:
```c
#include "firmware/headless/can/rusefi_can_core.h"
#include "firmware/headless/sensors/rusefi_headless_sensors_snapshot.h"
#include "firmware/headless/esm/rusefi_headless_esm.h"
```

The unit_tests `Makefile` sets `PROJECT_DIR = ../firmware` and the `INCDIR` entries
expand to paths like `../firmware/headless`, `../firmware/headless/sensors`, etc.
None of these would resolve an include path starting with `firmware/`. The path
`firmware/headless/can/rusefi_can_core.h` would only resolve if the **repo root**
(parent of both `unit_tests/` and `firmware/`) were in the include path — but it is not.

**Fix (Option A — Preferred):** Change the `#include` directives to use just the
header filename (which is how all other test files in the project work):
```c
#include "rusefi_can_core.h"
#include "rusefi_headless_sensors_snapshot.h"
#include "rusefi_headless_esm.h"
```
This requires that the corresponding directories are in `HEADLESS_INCDIR` (see Issue #3).

**Fix (Option B):** Add `..` (the repo root) to `INCDIR` in `unit_tests/Makefile`.
This is less clean and not consistent with the rest of the project.

---

### 3. Missing `headless/can` Directory in HEADLESS_INCDIR

**Affected file:** `firmware/headless/headless.mk`

**Problem:**
`HEADLESS_INCDIR` includes:
```makefile
$(PROJECT_DIR)/headless
$(PROJECT_DIR)/headless/sensors
$(PROJECT_DIR)/headless/esm
```
But does **not** include `$(PROJECT_DIR)/headless/can`. This means
`rusefi_can_core.h` cannot be found via standard header search paths during
compilation, causing a build failure for any file that `#include`s it by filename.

**Fix:** Add `$(PROJECT_DIR)/headless/can` to `HEADLESS_INCDIR` in
`firmware/headless/headless.mk`.

---

### 4. `rusefi_can_core.c` Not Compiled in Unit Tests

**Affected files:**
- `firmware/headless/headless.mk` (HEADLESS_SRC)
- `unit_tests/Makefile` (CSRC)

**Problem:**
`rusefi_can_core.c` is listed in `firmware/hw_layer/hw_layer.mk` under
`HW_LAYER_EGT` (and transitively in `HW_LAYER_EMS`), but **not** in
`firmware/headless/headless.mk`'s `HEADLESS_SRC`.

The unit_tests `Makefile` CSRC includes `$(HEADLESS_SRC)` but does **not** include
`$(HW_LAYER_EMS)` (only CPPSRC includes `$(HW_LAYER_EMS_CPP)`). Therefore
`rusefi_can_core.c` is never compiled in the unit test build, causing **linker
errors** (undefined references) when `test_headless_can_core.cpp` calls functions
like `rusefi_can_core_init()`, `rusefi_can_core_dispatch_rx()`, etc.

**Fix:** Add `$(PROJECT_DIR)/headless/can/rusefi_can_core.c` to `HEADLESS_SRC` in
`firmware/headless/headless.mk`. This is more correct than adding it to HW_LAYER
since the CAN core is a portable/headless component.

---

### 5. Missing `test_data_structures/` Directory

**Affected file:** `unit_tests/Makefile` (line in INCDIR)

**Problem:**
The `INCDIR` block includes `test_data_structures` as an include directory, but this
directory does not exist on disk. While GNU Make/GCC silently ignore missing `-I`
paths, any future code expecting to find headers there will fail. This is a latent
gap.

**Fix:** Either create the directory (if test data structure headers are planned) or
remove it from INCDIR to keep the build configuration clean.

---

## Moderate Issues

### 6. Architectural Inconsistency: CAN Core Placement

**Affected files:**
- `firmware/hw_layer/hw_layer.mk` — lists `headless/can/rusefi_can_core.c` in
  `HW_LAYER_EGT`
- `firmware/headless/headless.mk` — does not list it

**Problem:**
The CAN core file physically lives under `firmware/headless/can/` (a headless/portable
module) but is registered as a source file in `hw_layer.mk` (hardware abstraction
layer). This creates confusion about the module's ownership and makes it easy for
build configurations to miss including it (as seen in Issue #4 with unit tests).

**Fix:** Move the CAN core source registration from `hw_layer.mk` to `headless.mk`
where it architecturally belongs. Update any other Makefile that depends on
`HW_LAYER_EGT` accordingly.

---

### 7. Java Console — No Dependency Management Tool

**Affected file:** `java_console/build.xml`

**Problem:**
All Java dependencies are vendored JAR files in `java_console/lib/`. There is no
Maven/Gradle build system, so:
- Dependency versions are frozen and potentially outdated (e.g., `log4j.jar` with
  no version suffix — could be a pre-2.x version with known CVEs).
- No transitive dependency resolution.
- No automated vulnerability scanning.

While all referenced JARs are present on disk, this is a maintenance risk.

**Recommendation:** Consider migrating to Gradle or Maven for the Java components.
At minimum, audit `log4j.jar` for the Log4Shell vulnerability (CVE-2021-44228).

---

### 8. `rusefi.mk` Auto-Submodule Logic May Cause Confusing Errors

**Affected file:** `firmware/rusefi.mk`

**Problem:**
```makefile
ifeq ("$(wildcard $(RULESFILE))","")
$(info Invoking "git submodule update --init")
$(shell git submodule update --init)
$(error Please run 'make' again)
endif
```
This auto-invokes `git submodule update --init` during Makefile parsing. If the
workspace is not a proper git repo (e.g., downloaded as a zip, or in a CI container
without git history), this silently fails and then prints an unhelpful `$(error)`
message telling the user to "run make again" — which will also fail.

**Recommendation:** Add a check for `.git` directory existence before attempting
submodule init, and provide a clearer error message.

---

## Low-Severity / Informational

### 9. Firmware Build Requires ARM Cross-Compiler Toolchain

The firmware `Makefile` uses `arm-none-eabi-gcc` toolchain (`TRGT = arm-none-eabi-`).
This is expected for STM32 targets but means firmware cannot be built in a standard
x86 development environment without installing the ARM GCC toolchain.

### 10. Simulator Requires 32-bit Libraries on Linux

`simulator/Makefile` adds `-m32` flag on non-Windows platforms, requiring 32-bit
development libraries (`gcc-multilib`, `g++-multilib`) to be installed.

### 11. Unit Test `efifeatures.h` References `rusefi_true.h`

The file `unit_tests/efifeatures.h` includes `rusefi_true.h` which lives at
`firmware/util/rusefi_true.h`. This resolves correctly because `$(PROJECT_DIR)/util`
is in `INCDIR`. No issue currently, but worth noting as a cross-module dependency.

---

## Summary Table

| # | Severity | Component | File(s) | Issue |
|---|----------|-----------|---------|-------|
| 1 | **CRITICAL** | All | `.gitmodules`, submodule dirs | Empty submodules block all builds |
| 2 | **CRITICAL** | Unit Tests | `tests/test_headless_*.cpp` | Wrong include path prefix `firmware/` |
| 3 | **CRITICAL** | Build System | `headless/headless.mk` | Missing `headless/can` in INCDIR |
| 4 | **CRITICAL** | Unit Tests | `headless.mk`, `unit_tests/Makefile` | `rusefi_can_core.c` not compiled for tests |
| 5 | **MODERATE** | Unit Tests | `unit_tests/Makefile` | `test_data_structures/` dir missing |
| 6 | **MODERATE** | Architecture | `hw_layer.mk`, `headless.mk` | CAN core listed in wrong `.mk` |
| 7 | **LOW** | Java Console | `build.xml`, `lib/*.jar` | No dependency management; audit log4j |
| 8 | **LOW** | Build System | `rusefi.mk` | Confusing auto-submodule error message |
| 9 | **INFO** | Firmware | `firmware/Makefile` | Requires ARM cross-compiler |
| 10 | **INFO** | Simulator | `simulator/Makefile` | Requires 32-bit libs on Linux |
| 11 | **INFO** | Unit Tests | `efifeatures.h` | Cross-module include dependency |

---

## Recommended Priority Order for Fixes

1. Populate git submodules (Issue #1) — unblocks all builds
2. Fix headless test `#include` paths (Issue #2) — change to filename-only includes
3. Add `headless/can` to `HEADLESS_INCDIR` (Issue #3) — enables CAN header resolution
4. Add `rusefi_can_core.c` to `HEADLESS_SRC` (Issue #4) — fixes unit test linking
5. Clean up `test_data_structures` reference (Issue #5) — remove dead INCDIR entry
6. Consolidate CAN core `.mk` ownership (Issue #6) — architecture hygiene
