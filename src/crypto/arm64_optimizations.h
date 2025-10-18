// Copyright (c) 2017-2022 Fuego Developers
//
// ARM64-specific performance optimizations for Fuego
// This file contains ARM64-optimized implementations of critical functions
//

#pragma once

#if defined(__aarch64__)

#include <arm_neon.h>
#include <stdint.h>

// ARM64-specific prefetch macros for better cache performance
#define ARM64_PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
#define ARM64_PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)

// Optimized memory operations for ARM64
static inline void arm64_memcpy_optimized(void* dst, const void* src, size_t size) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    
    // Use 128-bit NEON loads/stores for large copies
    while (size >= 16) {
        uint8x16_t data = vld1q_u8(s);
        vst1q_u8(d, data);
        d += 16;
        s += 16;
        size -= 16;
    }
    
    // Handle remaining bytes
    while (size--) {
        *d++ = *s++;
    }
}

// Optimized XOR operations using NEON
static inline void arm64_xor_blocks_optimized(uint8_t* a, const uint8_t* b) {
    uint64x2_t va = vld1q_u64((const uint64_t*)a);
    uint64x2_t vb = vld1q_u64((const uint64_t*)b);
    uint64x2_t result = veorq_u64(va, vb);
    vst1q_u64((uint64_t*)a, result);
}

// Optimized 64-bit XOR
static inline void arm64_xor64_optimized(uint64_t* a, const uint64_t b) {
    *a ^= b;
}

// CPU feature detection for runtime optimization
static inline int arm64_has_crypto_extensions(void) {
    uint64_t features;
    __asm__("mrs %0, ID_AA64ISAR0_EL1" : "=r" (features));
    return (features & (1ULL << 4)) != 0; // AES support
}

static inline int arm64_has_neon(void) {
    uint64_t features;
    __asm__("mrs %0, ID_AA64PFR0_EL1" : "=r" (features));
    return (features & (0xF << 20)) == 0; // NEON support
}

// Optimized batch operations for better NEON utilization
#define ARM64_BATCH_SIZE 4

// Prefetch macros for different access patterns
#define ARM64_PREFETCH_T0(addr) __builtin_prefetch((addr), 0, 0)
#define ARM64_PREFETCH_T1(addr) __builtin_prefetch((addr), 0, 1)
#define ARM64_PREFETCH_T2(addr) __builtin_prefetch((addr), 0, 2)
#define ARM64_PREFETCH_NTA(addr) __builtin_prefetch((addr), 0, 3)

#define ARM64_PREFETCH_WT0(addr) __builtin_prefetch((addr), 1, 0)
#define ARM64_PREFETCH_WT1(addr) __builtin_prefetch((addr), 1, 1)
#define ARM64_PREFETCH_WT2(addr) __builtin_prefetch((addr), 1, 2)

#endif // __aarch64__