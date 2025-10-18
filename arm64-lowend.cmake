# ARM64 Low-End Device Configuration
# Optimized for devices with limited memory and processing power
# while preserving 2MB scratchpad and mining algorithm integrity

include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

# Set target architecture
set(CMAKE_SYSTEM_PROCESSOR "aarch64")
set(ARCH "arm64")

# ARM64 specific optimizations
set(ARM64 1)
set(ARM8 1)

# Compiler flags for low-end ARM64 devices
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+fp+simd+crypto")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+fp+simd+crypto")

# Size optimization flags
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG -ffunction-sections -fdata-sections")

# Link-time optimization
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")

# Linker flags for size optimization
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Wl,--gc-sections -Wl,--strip-all")

# Memory optimization flags
add_definitions(-DFUEGO_LOWEND_DEVICE=1)
add_definitions(-DFUEGO_ARM64_OPTIMIZED=1)

# Disable non-essential features for low-end devices
add_definitions(-DFUEGO_DISABLE_DEBUG_COMMANDS=1)
add_definitions(-DFUEGO_MINIMAL_LOGGING=1)

# ARM64 specific workarounds
CHECK_CXX_COMPILER_FLAG(-mfix-cortex-a53-835769 CXX_ACCEPTS_MFIX_CORTEX_A53_835769)
CHECK_CXX_COMPILER_FLAG(-mfix-cortex-a53-843419 CXX_ACCEPTS_MFIX_CORTEX_A53_843419)

if(CXX_ACCEPTS_MFIX_CORTEX_A53_835769)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-835769")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfix-cortex-a53-835769")
endif()

if(CXX_ACCEPTS_MFIX_CORTEX_A53_843419)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-843419")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfix-cortex-a53-843419")
endif()

# Memory alignment optimizations
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -falign-functions=16 -falign-loops=16")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -falign-functions=16 -falign-loops=16")

# Reduce stack usage
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-usage -Wstack-usage=4096")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-usage -Wstack-usage=4096")

message(STATUS "ARM64 Low-End Device configuration loaded")
message(STATUS "Target: ARM64 with NEON and Crypto extensions")
message(STATUS "Optimization: Size-optimized build")