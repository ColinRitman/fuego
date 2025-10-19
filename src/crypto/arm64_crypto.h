// Copyright (c) 2024 Fuego Developers
// ARM64 NEON Crypto Optimizations for Low-End Devices

#pragma once

#include <arm_neon.h>
#include <stdint.h>
#include <stddef.h>

#ifdef FUEGO_ARM64_OPTIMIZED

namespace Crypto {
namespace ARM64 {

// ARM64 NEON ChaCha8 implementation
void chacha8_arm64_neon(const void* data, size_t length, const void* key, const void* iv, void* cipher);

// ARM64 NEON hash functions
void cn_fast_hash_arm64_neon(const void* data, size_t length, void* hash);

// Memory operations optimized for ARM64
void* align_arm64(void* ptr);
void memcpy_arm64_neon(void* dest, const void* src, size_t n);

} // namespace ARM64
} // namespace Crypto

#endif // FUEGO_ARM64_OPTIMIZED