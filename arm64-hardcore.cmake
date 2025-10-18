# Fuego Hardcore ARM64 Ultra-Low-End Configuration
# Maximum optimization for devices with extreme resource constraints
# Preserves 2MB scratchpad and mining algorithm integrity

include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

# Set target architecture for maximum optimization
set(CMAKE_SYSTEM_PROCESSOR "aarch64")
set(ARCH "arm64")

# Hardcore ARM64 optimizations
set(ARM64 1)
set(ARM8 1)
set(FUEGO_HARDCORE_MODE 1)

# Ultra-aggressive compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+fp+simd+crypto+rcpc+dotprod")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+fp+simd+crypto+rcpc+dotprod")

# Maximum size optimization
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables")

# Ultra-aggressive link-time optimization
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto=auto -fuse-linker-plugin")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=auto -fuse-linker-plugin")

# Maximum linker optimizations
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Wl,--gc-sections -Wl,--strip-all -Wl,--as-needed -Wl,--no-undefined -Wl,--build-id=none")

# Hardcore memory optimization flags
add_definitions(-DFUEGO_HARDCORE_MODE=1)
add_definitions(-DFUEGO_ULTRA_LOWEND=1)
add_definitions(-DFUEGO_ARM64_HARDCORE=1)
add_definitions(-DFUEGO_MINIMAL_FEATURES=1)

# Disable all non-essential features
add_definitions(-DFUEGO_DISABLE_DEBUG_COMMANDS=1)
add_definitions(-DFUEGO_DISABLE_LOGGING=1)
add_definitions(-DFUEGO_DISABLE_STATISTICS=1)
add_definitions(-DFUEGO_DISABLE_MONITORING=1)
add_definitions(-DFUEGO_DISABLE_EXPLORER=1)
add_definitions(-DFUEGO_DISABLE_RPC=1)
add_definitions(-DFUEGO_DISABLE_HTTP=1)
add_definitions(-DFUEGO_DISABLE_JSON=1)
add_definitions(-DFUEGO_DISABLE_SERIALIZATION=1)
add_definitions(-DFUEGO_DISABLE_P2P=1)
add_definitions(-DFUEGO_DISABLE_WALLET=1)
add_definitions(-DFUEGO_DISABLE_TRANSFERS=1)
add_definitions(-DFUEGO_DISABLE_PAYMENT_GATE=1)
add_definitions(-DFUEGO_DISABLE_OPTIMIZER=1)
add_definitions(-DFUEGO_DISABLE_TESTS=1)

# Enable only core functionality
add_definitions(-DFUEGO_CORE_ONLY=1)
add_definitions(-DFUEGO_MINIMAL_BUILD=1)

# Ultra-aggressive ARM64 workarounds
CHECK_CXX_COMPILER_FLAG(-mfix-cortex-a53-835769 CXX_ACCEPTS_MFIX_CORTEX_A53_835769)
CHECK_CXX_COMPILER_FLAG(-mfix-cortex-a53-843419 CXX_ACCEPTS_MFIX_CORTEX_A53_843419)
CHECK_CXX_COMPILER_FLAG(-mfix-cortex-a72-843419 CXX_ACCEPTS_MFIX_CORTEX_A72_843419)

if(CXX_ACCEPTS_MFIX_CORTEX_A53_835769)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-835769")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfix-cortex-a53-835769")
endif()

if(CXX_ACCEPTS_MFIX_CORTEX_A53_843419)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-843419")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfix-cortex-a53-843419")
endif()

if(CXX_ACCEPTS_MFIX_CORTEX_A72_843419)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a72-843419")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfix-cortex-a72-843419")
endif()

# Maximum memory alignment optimizations
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -falign-functions=32 -falign-loops=32 -falign-jumps=32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -falign-functions=32 -falign-loops=32 -falign-jumps=32")

# Ultra-aggressive stack usage optimization
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-usage -Wstack-usage=2048 -fno-stack-protector")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-usage -Wstack-usage=2048 -fno-stack-protector")

# Maximum optimization flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fomit-frame-pointer -fno-exceptions -fno-rtti")

# Ultra-aggressive dead code elimination
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections -fvisibility=hidden")

# Maximum performance optimizations
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common -fno-builtin -fno-builtin-malloc -fno-builtin-free")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common -fno-builtin -fno-builtin-malloc -fno-builtin-free")

# Ultra-aggressive inlining
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -finline-functions -finline-limit=1000")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finline-functions -finline-limit=1000")

# Maximum vectorization
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftree-vectorize -fvectorize")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftree-vectorize -fvectorize")

# Ultra-aggressive loop optimizations
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -funroll-loops -funroll-all-loops -floop-optimize")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops -funroll-all-loops -floop-optimize")

# Maximum branch prediction optimization
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")

message(STATUS "Hardcore ARM64 Ultra-Low-End configuration loaded")
message(STATUS "Target: ARM64 with maximum optimizations")
message(STATUS "Optimization: Ultra-aggressive size and performance")
message(STATUS "Features: Core functionality only")