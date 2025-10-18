// Copyright (c) 2024 Fuego Developers
// Hardcore ARM64 Ultra-Aggressive Cryptographic Optimizations
// Maximum performance for extreme resource constraints

#pragma once

#include <arm_neon.h>
#include <stdint.h>
#include <stddef.h>

#ifdef FUEGO_ARM64_HARDCORE

namespace Crypto {
namespace HardcoreARM64 {

// Ultra-aggressive ARM64 NEON ChaCha8 implementation
HARDCORE_FORCE_INLINE void chacha8_hardcore_arm64_neon(const void* data, size_t length, const void* key, const void* iv, void* cipher);

// Ultra-aggressive ARM64 NEON hash functions
HARDCORE_FORCE_INLINE void cn_fast_hash_hardcore_arm64_neon(const void* data, size_t length, void* hash);
HARDCORE_FORCE_INLINE void cn_slow_hash_v0_hardcore_arm64_neon(const void* data, size_t length, void* hash);
HARDCORE_FORCE_INLINE void cn_slow_hash_v1_hardcore_arm64_neon(const void* data, size_t length, void* hash);
HARDCORE_FORCE_INLINE void cn_slow_hash_v2_hardcore_arm64_neon(const void* data, size_t length, void* hash);

// Ultra-aggressive ARM64 crypto operations
HARDCORE_FORCE_INLINE void aes_encrypt_hardcore_arm64_neon(const uint8_t* input, const uint8_t* key, uint8_t* output, size_t blocks);
HARDCORE_FORCE_INLINE void aes_decrypt_hardcore_arm64_neon(const uint8_t* input, const uint8_t* key, uint8_t* output, size_t blocks);

// Ultra-aggressive ARM64 keccak implementation
HARDCORE_FORCE_INLINE void keccak_hardcore_arm64_neon(const void* input, size_t input_len, void* output, size_t output_len);

// Ultra-aggressive ARM64 blake2b implementation
HARDCORE_FORCE_INLINE void blake2b_hardcore_arm64_neon(const void* input, size_t input_len, void* output, size_t output_len);

// Ultra-aggressive memory operations
HARDCORE_FORCE_INLINE void memcpy_hardcore_arm64_neon(void* dest, const void* src, size_t n);
HARDCORE_FORCE_INLINE void memset_hardcore_arm64_neon(void* s, int c, size_t n);
HARDCORE_FORCE_INLINE int memcmp_hardcore_arm64_neon(const void* s1, const void* s2, size_t n);

// Ultra-aggressive ARM64 optimizations
HARDCORE_FORCE_INLINE void* align_hardcore_arm64(void* ptr) {
    return reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(ptr) + 31) & ~31);
}

HARDCORE_FORCE_INLINE bool is_hardcore_arm64_aligned(const void* ptr) {
    return (reinterpret_cast<uintptr_t>(ptr) & 31) == 0;
}

// Ultra-aggressive ARM64 NEON vector operations
HARDCORE_FORCE_INLINE uint32x4_t vaddq_u32_hardcore(uint32x4_t a, uint32x4_t b) {
    return vaddq_u32(a, b);
}

HARDCORE_FORCE_INLINE uint32x4_t veorq_u32_hardcore(uint32x4_t a, uint32x4_t b) {
    return veorq_u32(a, b);
}

HARDCORE_FORCE_INLINE uint32x4_t vshlq_n_u32_hardcore(uint32x4_t a, int n) {
    return vshlq_n_u32(a, n);
}

HARDCORE_FORCE_INLINE uint32x4_t vrev64q_u32_hardcore(uint32x4_t a) {
    return vrev64q_u32(a);
}

// Ultra-aggressive ARM64 crypto extensions
HARDCORE_FORCE_INLINE uint8x16_t vaeseq_u8_hardcore(uint8x16_t data, uint8x16_t key) {
    return vaeseq_u8(data, key);
}

HARDCORE_FORCE_INLINE uint8x16_t vaesmcq_u8_hardcore(uint8x16_t data) {
    return vaesmcq_u8(data);
}

HARDCORE_FORCE_INLINE uint8x16_t vaesdq_u8_hardcore(uint8x16_t data, uint8x16_t key) {
    return vaesdq_u8(data, key);
}

HARDCORE_FORCE_INLINE uint8x16_t vaesimcq_u8_hardcore(uint8x16_t data) {
    return vaesimcq_u8(data);
}

// Ultra-aggressive ARM64 SHA operations
HARDCORE_FORCE_INLINE uint32x4_t vsha256hq_u32_hardcore(uint32x4_t hash_abcd, uint32x4_t hash_efgh, uint32x4_t wk) {
    return vsha256hq_u32(hash_abcd, hash_efgh, wk);
}

HARDCORE_FORCE_INLINE uint32x4_t vsha256h2q_u32_hardcore(uint32x4_t hash_efgh, uint32x4_t hash_abcd, uint32x4_t wk) {
    return vsha256h2q_u32(hash_efgh, hash_abcd, wk);
}

HARDCORE_FORCE_INLINE uint32x4_t vsha256su0q_u32_hardcore(uint32x4_t w0_3, uint32x4_t w4_7) {
    return vsha256su0q_u32(w0_3, w4_7);
}

HARDCORE_FORCE_INLINE uint32x4_t vsha256su1q_u32_hardcore(uint32x4_t tw0_3, uint32x4_t w8_11, uint32x4_t w12_15) {
    return vsha256su1q_u32(tw0_3, w8_11, w12_15);
}

// Ultra-aggressive ARM64 dot product operations
HARDCORE_FORCE_INLINE uint32x4_t vdotq_u32_hardcore(uint32x4_t a, uint8x16_t b, uint8x16_t c) {
    return vdotq_u32(a, b, c);
}

HARDCORE_FORCE_INLINE uint32x2_t vdot_u32_hardcore(uint32x2_t a, uint8x8_t b, uint8x8_t c) {
    return vdot_u32(a, b, c);
}

// Ultra-aggressive ARM64 RCPC operations
HARDCORE_FORCE_INLINE uint32x4_t vldap1q_u32_hardcore(const uint32_t* ptr) {
    return vldap1q_u32(ptr);
}

HARDCORE_FORCE_INLINE void vstap1q_u32_hardcore(uint32_t* ptr, uint32x4_t val) {
    vstap1q_u32(ptr, val);
}

} // namespace HardcoreARM64
} // namespace Crypto

#endif // FUEGO_ARM64_HARDCORE