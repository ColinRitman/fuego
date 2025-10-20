// Copyright (c) 2024 Fuego Developers
// Low-End Device Configuration Header
// Optimized for ARM64 devices with limited resources

#pragma once

#ifdef FUEGO_LOWEND_DEVICE

// Memory optimization constants
namespace FuegoLowEnd {
    // Reduced buffer sizes for low-end devices
    constexpr size_t LOWEND_MAX_CONNECTIONS = 4;           // Reduced from 8
    constexpr size_t LOWEND_MAX_PEER_LIST = 100;           // Reduced from 1000
    constexpr size_t LOWEND_MAX_TX_POOL_SIZE = 1000;       // Reduced transaction pool
    constexpr size_t LOWEND_MAX_BLOCK_CACHE = 50;          // Reduced block cache
    constexpr size_t LOWEND_MAX_WALLET_CACHE = 100;        // Reduced wallet cache
    
    // Threading optimizations
    constexpr size_t LOWEND_MAX_THREADS = 2;               // Reduced thread count
    constexpr size_t LOWEND_IO_THREADS = 1;                // Single IO thread
    
    // Memory pool sizes
    constexpr size_t LOWEND_SMALL_POOL_SIZE = 1024;        // 1KB small object pool
    constexpr size_t LOWEND_MEDIUM_POOL_SIZE = 4096;       // 4KB medium object pool
    constexpr size_t LOWEND_LARGE_POOL_SIZE = 16384;       // 16KB large object pool
    
    // Network optimizations
    constexpr size_t LOWEND_MAX_PACKET_SIZE = 1048576;     // 1MB max packet (reduced from 50MB)
    constexpr size_t LOWEND_CONNECTION_TIMEOUT = 10000;    // 10s timeout (increased from 5s)
    
    // Logging optimizations
    constexpr size_t LOWEND_MAX_LOG_LEVEL = 2;             // Only ERROR and WARNING
    constexpr size_t LOWEND_LOG_BUFFER_SIZE = 1024;        // 1KB log buffer
}

// Feature flags for low-end devices
#define FUEGO_DISABLE_DEBUG_COMMANDS
#define FUEGO_MINIMAL_LOGGING
#define FUEGO_REDUCED_MEMORY_FOOTPRINT
#define FUEGO_OPTIMIZED_FOR_ARM64

// Memory alignment for ARM64
#define FUEGO_ARM64_ALIGNMENT 16

// Conditional compilation macros
#define IF_LOWEND(code) code
#define IF_NOT_LOWEND(code)
#define LOWEND_CONSTANT(name) FuegoLowEnd::name

#else
// Standard build macros
#define IF_LOWEND(code)
#define IF_NOT_LOWEND(code) code
#define LOWEND_CONSTANT(name) name

#endif // FUEGO_LOWEND_DEVICE