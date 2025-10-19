// Copyright (c) 2024 Fuego Developers
// Advanced ARM64 NEON Crypto Optimizations for Low-End Devices
// Phase 2: Advanced optimizations building on Phase 1 foundation

#pragma once

#include <arm_neon.h>
#include <stdint.h>
#include <stddef.h>

#ifdef FUEGO_ARM64_OPTIMIZED

namespace Crypto {
namespace ARM64 {
namespace Advanced {

// Advanced ChaCha8 with NEON vectorization
void chacha8_advanced_neon(const void* data, size_t length, const void* key, const void* iv, void* cipher);

// Advanced hash functions with NEON acceleration
void cn_fast_hash_advanced_neon(const void* data, size_t length, void* hash);
void cn_slow_hash_v0_advanced_neon(const void* data, size_t length, void* hash);
void cn_slow_hash_v1_advanced_neon(const void* data, size_t length, void* hash);
void cn_slow_hash_v2_advanced_neon(const void* data, size_t length, void* hash);

// Advanced AES operations with NEON
void aes_encrypt_advanced_neon(const uint8_t* input, const uint8_t* key, uint8_t* output, size_t blocks);
void aes_decrypt_advanced_neon(const uint8_t* input, const uint8_t* key, uint8_t* output, size_t blocks);

// Advanced Keccak with NEON
void keccak_advanced_neon(const void* input, size_t input_len, void* output, size_t output_len);

// Advanced Blake2b with NEON
void blake2b_advanced_neon(const void* input, size_t input_len, void* output, size_t output_len);

// Advanced memory operations
void* align_advanced_arm64(void* ptr, size_t alignment);
void memcpy_advanced_neon(void* dest, const void* src, size_t n);
void memset_advanced_neon(void* s, int c, size_t n);
int memcmp_advanced_neon(const void* s1, const void* s2, size_t n);

// Advanced NEON vector operations
inline uint32x4_t vaddq_u32_advanced(uint32x4_t a, uint32x4_t b) {
    return vaddq_u32(a, b);
}

inline uint32x4_t veorq_u32_advanced(uint32x4_t a, uint32x4_t b) {
    return veorq_u32(a, b);
}

inline uint32x4_t vshlq_n_u32_advanced(uint32x4_t a, int n) {
    return vshlq_n_u32(a, n);
}

inline uint32x4_t vrev64q_u32_advanced(uint32x4_t a) {
    return vrev64q_u32(a);
}

// Advanced crypto extensions
inline uint8x16_t vaeseq_u8_advanced(uint8x16_t data, uint8x16_t key) {
    return vaeseq_u8(data, key);
}

inline uint8x16_t vaesmcq_u8_advanced(uint8x16_t data) {
    return vaesmcq_u8(data);
}

inline uint8x16_t vaesdq_u8_advanced(uint8x16_t data, uint8x16_t key) {
    return vaesdq_u8(data, key);
}

inline uint8x16_t vaesimcq_u8_advanced(uint8x16_t data) {
    return vaesimcq_u8(data);
}

// Advanced SHA operations
inline uint32x4_t vsha256hq_u32_advanced(uint32x4_t hash_abcd, uint32x4_t hash_efgh, uint32x4_t wk) {
    return vsha256hq_u32(hash_abcd, hash_efgh, wk);
}

inline uint32x4_t vsha256h2q_u32_advanced(uint32x4_t hash_efgh, uint32x4_t hash_abcd, uint32x4_t wk) {
    return vsha256h2q_u32(hash_efgh, hash_abcd, wk);
}

inline uint32x4_t vsha256su0q_u32_advanced(uint32x4_t w0_3, uint32x4_t w4_7) {
    return vsha256su0q_u32(w0_3, w4_7);
}

inline uint32x4_t vsha256su1q_u32_advanced(uint32x4_t tw0_3, uint32x4_t w8_11, uint32x4_t w12_15) {
    return vsha256su1q_u32(tw0_3, w8_11, w12_15);
}

// Advanced memory prefetching
void prefetch_read_arm64(const void* addr);
void prefetch_write_arm64(const void* addr);

// Advanced cache management
void cache_flush_arm64(const void* addr, size_t size);
void cache_invalidate_arm64(const void* addr, size_t size);

} // namespace Advanced
} // namespace ARM64
} // namespace Crypto

#endif // FUEGO_ARM64_OPTIMIZED