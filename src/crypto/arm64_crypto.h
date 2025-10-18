// Copyright (c) 2024 Fuego Developers
// ARM64 NEON optimized cryptographic functions

#pragma once

#include <arm_neon.h>
#include <stdint.h>
#include <stddef.h>

#ifdef FUEGO_ARM64_OPTIMIZED

namespace Crypto {
namespace ARM64 {

// ARM64 NEON optimized ChaCha8 implementation
void chacha8_arm64_neon(const void* data, size_t length, const void* key, const void* iv, void* cipher);

// ARM64 NEON optimized hash functions
void cn_fast_hash_arm64_neon(const void* data, size_t length, void* hash);
void cn_slow_hash_v0_arm64_neon(const void* data, size_t length, void* hash);
void cn_slow_hash_v1_arm64_neon(const void* data, size_t length, void* hash);
void cn_slow_hash_v2_arm64_neon(const void* data, size_t length, void* hash);

// ARM64 optimized AES operations
void aes_encrypt_arm64_neon(const uint8_t* input, const uint8_t* key, uint8_t* output, size_t blocks);
void aes_decrypt_arm64_neon(const uint8_t* input, const uint8_t* key, uint8_t* output, size_t blocks);

// ARM64 optimized keccak implementation
void keccak_arm64_neon(const void* input, size_t input_len, void* output, size_t output_len);

// ARM64 optimized blake2b implementation
void blake2b_arm64_neon(const void* input, size_t input_len, void* output, size_t output_len);

// Memory alignment helpers for ARM64
inline void* align_arm64(void* ptr) {
    return reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(ptr) + 15) & ~15);
}

inline bool is_arm64_aligned(const void* ptr) {
    return (reinterpret_cast<uintptr_t>(ptr) & 15) == 0;
}

// ARM64 optimized memory operations
void memcpy_arm64_neon(void* dest, const void* src, size_t n);
void memset_arm64_neon(void* s, int c, size_t n);
int memcmp_arm64_neon(const void* s1, const void* s2, size_t n);

} // namespace ARM64
} // namespace Crypto

#endif // FUEGO_ARM64_OPTIMIZED