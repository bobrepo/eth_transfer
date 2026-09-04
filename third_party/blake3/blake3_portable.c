#include "blake3_impl.h"

void blake3_compress_in_place_portable(uint32_t cv[8],
                                       const uint8_t block[BLAKE3_BLOCK_LEN],
                                       uint8_t block_len, uint64_t counter,
                                       uint8_t flags) {
  uint32_t state[16];
  compress_pre(state, cv, block, block_len, counter, flags);
  cv[0] = state[0] ^ state[8];
  cv[1] = state[1] ^ state[9];
  cv[2] = state[2] ^ state[10];
  cv[3] = state[3] ^ state[11];
  cv[4] = state[4] ^ state[12];
  cv[5] = state[5] ^ state[13];
  cv[6] = state[6] ^ state[14];
  cv[7] = state[7] ^ state[15];
}

void blake3_compress_xof_portable(const uint32_t cv[8],
                                  const uint8_t block[BLAKE3_BLOCK_LEN],
                                  uint8_t block_len, uint64_t counter,
                                  uint8_t flags, uint8_t out[64]) {
  uint32_t state[16];
  compress_pre(state, cv, block, block_len, counter, flags);

  uint32_t out_words[16];
  out_words[0] = state[0] ^ state[8];
  out_words[1] = state[1] ^ state[9];
  out_words[2] = state[2] ^ state[10];
  out_words[3] = state[3] ^ state[11];
  out_words[4] = state[4] ^ state[12];
  out_words[5] = state[5] ^ state[13];
  out_words[6] = state[6] ^ state[14];
  out_words[7] = state[7] ^ state[15];
  out_words[8] = state[8] ^ cv[0];
  out_words[9] = state[9] ^ cv[1];
  out_words[10] = state[10] ^ cv[2];
  out_words[11] = state[11] ^ cv[3];
  out_words[12] = state[12] ^ cv[4];
  out_words[13] = state[13] ^ cv[5];
  out_words[14] = state[14] ^ cv[6];
  out_words[15] = state[15] ^ cv[7];

  memcpy(out, out_words, sizeof(out_words));
}
