// Copyright (c) 2024 Fuego Developers
// ARM64 NEON optimized ChaCha8 implementation

#include "arm64_crypto.h"

#ifdef FUEGO_ARM64_OPTIMIZED

#include <arm_neon.h>
#include <string.h>

namespace Crypto {
namespace ARM64 {

// ChaCha8 quarter round using ARM64 NEON
static inline void chacha8_quarter_round_neon(uint32x4_t* a, uint32x4_t* b, uint32x4_t* c, uint32x4_t* d) {
    *a = vaddq_u32(*a, *b);
    *d = veorq_u32(*d, *a);
    *d = vrev64q_u32(vshlq_n_u32(*d, 16));
    
    *c = vaddq_u32(*c, *d);
    *b = veorq_u32(*b, *c);
    *b = vrev64q_u32(vshlq_n_u32(*b, 12));
    
    *a = vaddq_u32(*a, *b);
    *d = veorq_u32(*d, *a);
    *d = vrev64q_u32(vshlq_n_u32(*d, 8));
    
    *c = vaddq_u32(*c, *d);
    *b = veorq_u32(*b, *c);
    *b = vrev64q_u32(vshlq_n_u32(*b, 7));
}

// ARM64 NEON optimized ChaCha8 block function
static void chacha8_block_neon(const uint32_t* key, const uint32_t* iv, uint32_t* output) {
    uint32x4_t state[4];
    
    // Initialize state
    state[0] = vld1q_u32((const uint32_t*)"expand 32-byte k");
    state[1] = vld1q_u32(key);
    state[2] = vld1q_u32(key + 4);
    state[3] = vld1q_u32(iv);
    
    // 8 rounds of ChaCha8
    for (int i = 0; i < 8; i++) {
        chacha8_quarter_round_neon(&state[0], &state[1], &state[2], &state[3]);
        chacha8_quarter_round_neon(&state[0], &state[1], &state[2], &state[3]);
    }
    
    // Add original state
    state[0] = vaddq_u32(state[0], vld1q_u32((const uint32_t*)"expand 32-byte k"));
    state[1] = vaddq_u32(state[1], vld1q_u32(key));
    state[2] = vaddq_u32(state[2], vld1q_u32(key + 4));
    state[3] = vaddq_u32(state[3], vld1q_u32(iv));
    
    // Store result
    vst1q_u32(output, state[0]);
    vst1q_u32(output + 4, state[1]);
    vst1q_u32(output + 8, state[2]);
    vst1q_u32(output + 12, state[3]);
}

void chacha8_arm64_neon(const void* data, size_t length, const void* key, const void* iv, void* cipher) {
    const uint8_t* input = static_cast<const uint8_t*>(data);
    uint8_t* output = static_cast<uint8_t*>(cipher);
    const uint32_t* key32 = static_cast<const uint32_t*>(key);
    const uint32_t* iv32 = static_cast<const uint32_t*>(iv);
    
    uint32_t block[16];
    uint32_t counter = 0;
    
    while (length > 0) {
        // Set counter
        uint32_t iv_copy[4];
        memcpy(iv_copy, iv32, 12);
        iv_copy[3] = counter++;
        
        // Generate block
        chacha8_block_neon(key32, iv_copy, block);
        
        // XOR with input
        size_t block_size = (length < 64) ? length : 64;
        for (size_t i = 0; i < block_size; i++) {
            output[i] = input[i] ^ static_cast<uint8_t>(block[i / 4] >> ((i % 4) * 8));
        }
        
        input += block_size;
        output += block_size;
        length -= block_size;
    }
}

} // namespace ARM64
} // namespace Crypto

#endif // FUEGO_ARM64_OPTIMIZED