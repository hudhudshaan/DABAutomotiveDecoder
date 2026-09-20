# ==============================================================================
# CMake Toolchain File for Odroid N2+ Target Platform (ARM Cortex-A73/A55)
# Architecture: aarch64-linux-gnu (ARMv8-A + NEON SIMD)
# ==============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-compilation tools
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_AR aarch64-linux-gnu-ar)
set(CMAKE_RANLIB aarch64-linux-gnu-ranlib)
set(CMAKE_STRIP aarch64-linux-gnu-strip)

# Target CPU tuning for Amlogic S922X (Odroid N2+)
set(CMAKE_C_FLAGS_INIT "-march=armv8-a+simd -mtune=cortex-a73.cortex-a53 -O3 -fno-common -fPIC")

# Automatically enable NEON SIMD for Odroid N2+
set(DAB_ENABLE_NEON ON CACHE BOOL "Enable ARM64 NEON optimizations" FORCE)

# Sysroot configuration (set to target sysroot if cross-compiling with full rootfs)
# set(CMAKE_SYSROOT /opt/odroid-n2-sysroot)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
