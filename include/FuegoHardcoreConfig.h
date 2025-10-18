// Copyright (c) 2024 Fuego Developers
// Hardcore Ultra-Low-End Device Configuration
// Maximum optimization for extreme resource constraints

#pragma once

#ifdef FUEGO_HARDCORE_MODE

// Ultra-aggressive memory optimization constants
namespace FuegoHardcore {
    // Extreme buffer size reductions
    constexpr size_t HARDCORE_MAX_CONNECTIONS = 1;           // Single connection only
    constexpr size_t HARDCORE_MAX_PEER_LIST = 10;            // Minimal peer list
    constexpr size_t HARDCORE_MAX_TX_POOL_SIZE = 100;        // Tiny transaction pool
    constexpr size_t HARDCORE_MAX_BLOCK_CACHE = 5;           // Minimal block cache
    constexpr size_t HARDCORE_MAX_WALLET_CACHE = 10;         // Tiny wallet cache
    
    // Ultra-minimal threading
    constexpr size_t HARDCORE_MAX_THREADS = 1;               // Single thread only
    constexpr size_t HARDCORE_IO_THREADS = 1;                // Single IO thread
    
    // Extreme memory pool sizes
    constexpr size_t HARDCORE_TINY_POOL_SIZE = 256;          // 256B tiny object pool
    constexpr size_t HARDCORE_SMALL_POOL_SIZE = 512;         // 512B small object pool
    constexpr size_t HARDCORE_MEDIUM_POOL_SIZE = 1024;       // 1KB medium object pool
    
    // Ultra-minimal network settings
    constexpr size_t HARDCORE_MAX_PACKET_SIZE = 65536;       // 64KB max packet
    constexpr size_t HARDCORE_CONNECTION_TIMEOUT = 30000;    // 30s timeout
    constexpr size_t HARDCORE_MAX_RETRIES = 3;               // Minimal retries
    
    // Disable all non-essential features
    constexpr bool HARDCORE_DISABLE_LOGGING = true;
    constexpr bool HARDCORE_DISABLE_STATISTICS = true;
    constexpr bool HARDCORE_DISABLE_MONITORING = true;
    constexpr bool HARDCORE_DISABLE_EXPLORER = true;
    constexpr bool HARDCORE_DISABLE_RPC = true;
    constexpr bool HARDCORE_DISABLE_HTTP = true;
    constexpr bool HARDCORE_DISABLE_JSON = true;
    constexpr bool HARDCORE_DISABLE_SERIALIZATION = true;
    constexpr bool HARDCORE_DISABLE_P2P = true;
    constexpr bool HARDCORE_DISABLE_WALLET = true;
    constexpr bool HARDCORE_DISABLE_TRANSFERS = true;
    constexpr bool HARDCORE_DISABLE_PAYMENT_GATE = true;
    constexpr bool HARDCORE_DISABLE_OPTIMIZER = true;
    constexpr bool HARDCORE_DISABLE_TESTS = true;
    
    // Core functionality only
    constexpr bool HARDCORE_CORE_ONLY = true;
    constexpr bool HARDCORE_MINIMAL_BUILD = true;
    
    // Ultra-minimal memory limits
    constexpr size_t HARDCORE_MAX_MEMORY_USAGE = 8388608;    // 8MB maximum
    constexpr size_t HARDCORE_MAX_STACK_SIZE = 2048;         // 2KB stack limit
    constexpr size_t HARDCORE_MAX_HEAP_SIZE = 4194304;       // 4MB heap limit
    
    // Ultra-aggressive optimizations
    constexpr bool HARDCORE_ULTRA_OPTIMIZE = true;
    constexpr bool HARDCORE_ARM64_NEON = true;
    constexpr bool HARDCORE_ARM64_CRYPTO = true;
    constexpr bool HARDCORE_ARM64_DOTPROD = true;
    constexpr bool HARDCORE_ARM64_RCPC = true;
}

// Hardcore feature flags
#define FUEGO_DISABLE_DEBUG_COMMANDS
#define FUEGO_DISABLE_LOGGING
#define FUEGO_DISABLE_STATISTICS
#define FUEGO_DISABLE_MONITORING
#define FUEGO_DISABLE_EXPLORER
#define FUEGO_DISABLE_RPC
#define FUEGO_DISABLE_HTTP
#define FUEGO_DISABLE_JSON
#define FUEGO_DISABLE_SERIALIZATION
#define FUEGO_DISABLE_P2P
#define FUEGO_DISABLE_WALLET
#define FUEGO_DISABLE_TRANSFERS
#define FUEGO_DISABLE_PAYMENT_GATE
#define FUEGO_DISABLE_OPTIMIZER
#define FUEGO_DISABLE_TESTS

// Core functionality only
#define FUEGO_CORE_ONLY
#define FUEGO_MINIMAL_BUILD
#define FUEGO_ULTRA_LOWEND
#define FUEGO_ARM64_HARDCORE

// Memory alignment for maximum ARM64 performance
#define FUEGO_ARM64_ALIGNMENT 32
#define FUEGO_ARM64_CACHE_LINE 64

// Conditional compilation macros
#define IF_HARDCORE(code) code
#define IF_NOT_HARDCORE(code)
#define HARDCORE_CONSTANT(name) FuegoHardcore::name

// Ultra-aggressive inlining
#define HARDCORE_INLINE __attribute__((always_inline)) inline
#define HARDCORE_FORCE_INLINE __attribute__((always_inline, flatten)) inline

// Memory optimization macros
#define HARDCORE_ALIGNED __attribute__((aligned(32)))
#define HARDCORE_PACKED __attribute__((packed))
#define HARDCORE_HOT __attribute__((hot))
#define HARDCORE_COLD __attribute__((cold))

// Ultra-minimal error handling
#define HARDCORE_ASSERT(expr) ((void)0)
#define HARDCORE_VERIFY(expr) ((void)0)

// Disable all non-essential features
#define HARDCORE_DISABLE_FEATURE(feature) ((void)0)

#else
// Standard build macros
#define IF_HARDCORE(code)
#define IF_NOT_HARDCORE(code) code
#define HARDCORE_CONSTANT(name) name
#define HARDCORE_INLINE inline
#define HARDCORE_FORCE_INLINE inline
#define HARDCORE_ALIGNED
#define HARDCORE_PACKED
#define HARDCORE_HOT
#define HARDCORE_COLD
#define HARDCORE_ASSERT(expr) assert(expr)
#define HARDCORE_VERIFY(expr) assert(expr)
#define HARDCORE_DISABLE_FEATURE(feature) feature

#endif // FUEGO_HARDCORE_MODE