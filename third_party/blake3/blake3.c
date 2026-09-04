#include "blake3.h"
#include "blake3_impl.h"

const char *blake3_version(void) {
  return BLAKE3_VERSION_STRING;
}

static void chunk_state_init(blake3_chunk_state *self, const uint32_t key[8],
                             uint8_t flags) {
  memcpy(self->cv, key, sizeof(self->cv));
  self->chunk_counter = 0;
  memset(self->buf, 0, sizeof(self->buf));
  self->buf_len = 0;
  self->blocks_compressed = 0;
  self->flags = flags;
}

static void chunk_state_reset(blake3_chunk_state *self, const uint32_t key[8],
                              uint64_t chunk_counter) {
  memcpy(self->cv, key, sizeof(self->cv));
  self->chunk_counter = chunk_counter;
  self->buf_len = 0;
  self->blocks_compressed = 0;
}

static size_t chunk_state_len(const blake3_chunk_state *self) {
  return (BLAKE3_BLOCK_LEN * (size_t)self->blocks_compressed) +
         ((size_t)self->buf_len);
}

static size_t chunk_state_fill_buf(blake3_chunk_state *self,
                                   const uint8_t *input, size_t input_len) {
  size_t take = sizeof(self->buf) - ((size_t)self->buf_len);
  if (take > input_len) {
    take = input_len;
  }
  memcpy(self->buf + self->buf_len, input, take);
  self->buf_len = (uint8_t)(self->buf_len + take);
  return take;
}

static uint8_t chunk_state_maybe_start_flag(const blake3_chunk_state *self) {
  if (self->blocks_compressed == 0) {
    return CHUNK_START;
  } else {
    return 0;
  }
}

static void chunk_state_update(blake3_chunk_state *self, const uint8_t *input,
                               size_t input_len) {
  if (self->buf_len > 0) {
    size_t take = chunk_state_fill_buf(self, input, input_len);
    input += take;
    input_len -= take;
    if (input_len > 0) {
      blake3_compress_in_place_portable(
          self->cv, self->buf, BLAKE3_BLOCK_LEN, self->chunk_counter,
          self->flags | chunk_state_maybe_start_flag(self));
      self->blocks_compressed++;
      self->buf_len = 0;
      memset(self->buf, 0, sizeof(self->buf));
    }
  }

  while (input_len > BLAKE3_BLOCK_LEN) {
    blake3_compress_in_place_portable(
        self->cv, input, BLAKE3_BLOCK_LEN, self->chunk_counter,
        self->flags | chunk_state_maybe_start_flag(self));
    self->blocks_compressed++;
    input += BLAKE3_BLOCK_LEN;
    input_len -= BLAKE3_BLOCK_LEN;
  }

  size_t take = chunk_state_fill_buf(self, input, input_len);
  (void)take;
}

typedef struct {
  uint32_t input_cv[8];
  uint64_t counter;
  uint8_t block[BLAKE3_BLOCK_LEN];
  uint8_t block_len;
  uint8_t flags;
} output_t;

static output_t make_output(const uint32_t input_cv[8],
                            const uint8_t block[BLAKE3_BLOCK_LEN],
                            uint8_t block_len, uint64_t counter,
                            uint8_t flags) {
  output_t ret;
  memcpy(ret.input_cv, input_cv, sizeof(ret.input_cv));
  memcpy(ret.block, block, sizeof(ret.block));
  ret.block_len = block_len;
  ret.counter = counter;
  ret.flags = flags;
  return ret;
}

static output_t chunk_state_output(const blake3_chunk_state *self) {
  uint8_t block_flags =
      self->flags | chunk_state_maybe_start_flag(self) | CHUNK_END;
  return make_output(self->cv, self->buf, self->buf_len, self->chunk_counter,
                     block_flags);
}

static output_t parent_output(const uint8_t block[BLAKE3_BLOCK_LEN],
                              const uint32_t key[8], uint8_t flags) {
  return make_output(key, block, BLAKE3_BLOCK_LEN, 0, flags | PARENT);
}

static void output_chaining_value(const output_t *self, uint8_t cv[32]) {
  uint32_t cv_words[8];
  memcpy(cv_words, self->input_cv, sizeof(cv_words));
  blake3_compress_in_place_portable(cv_words, self->block, self->block_len,
                                    self->counter, self->flags);
  memcpy(cv, cv_words, 32);
}

static void output_root_bytes(const output_t *self, uint64_t seek, uint8_t *out,
                              size_t out_len) {
  uint64_t output_block_counter = seek / 64;
  size_t offset_within_block = (size_t)(seek % 64);
  uint8_t wide_buf[64];
  while (out_len > 0) {
    blake3_compress_xof_portable(self->input_cv, self->block, self->block_len,
                                 output_block_counter, self->flags | ROOT,
                                 wide_buf);
    size_t available_bytes = 64 - offset_within_block;
    size_t take = out_len;
    if (take > available_bytes) {
      take = available_bytes;
    }
    memcpy(out, wide_buf + offset_within_block, take);
    out += take;
    out_len -= take;
    output_block_counter++;
    offset_within_block = 0;
  }
}

static void hasher_init_base(blake3_hasher *self, const uint32_t key[8],
                             uint8_t flags) {
  memcpy(self->key, key, sizeof(self->key));
  chunk_state_init(&self->chunk, key, flags);
  self->cv_stack_len = 0;
}

void blake3_hasher_init(blake3_hasher *self) {
  hasher_init_base(self, IV, 0);
}

void blake3_hasher_init_keyed(blake3_hasher *self,
                              const uint8_t key[BLAKE3_KEY_LEN]) {
  uint32_t key_words[8];
  memcpy(key_words, key, sizeof(key_words));
  hasher_init_base(self, key_words, KEYED_HASH);
}

void blake3_hasher_init_derive_key_raw(blake3_hasher *self, const void *context,
                                       size_t context_len) {
  blake3_hasher context_hasher;
  hasher_init_base(&context_hasher, IV, DERIVE_KEY_CONTEXT);
  blake3_hasher_update(&context_hasher, context, context_len);
  uint8_t context_key[BLAKE3_KEY_LEN];
  blake3_hasher_finalize(&context_hasher, context_key, BLAKE3_KEY_LEN);
  uint32_t context_key_words[8];
  memcpy(context_key_words, context_key, sizeof(context_key_words));
  hasher_init_base(self, context_key_words, DERIVE_KEY_MATERIAL);
}

void blake3_hasher_init_derive_key(blake3_hasher *self, const char *context) {
  blake3_hasher_init_derive_key_raw(self, context, strlen(context));
}

void blake3_hasher_reset(blake3_hasher *self) {
  chunk_state_reset(&self->chunk, self->key, 0);
  self->cv_stack_len = 0;
}

static void hasher_push_cv(blake3_hasher *self, uint8_t new_cv[BLAKE3_OUT_LEN],
                           uint64_t chunk_counter) {
  while ((chunk_counter & 1) == 0) {
    uint8_t parent_block[BLAKE3_BLOCK_LEN];
    memcpy(parent_block, &self->cv_stack[(self->cv_stack_len - 1) * BLAKE3_OUT_LEN],
           BLAKE3_OUT_LEN);
    memcpy(parent_block + BLAKE3_OUT_LEN, new_cv, BLAKE3_OUT_LEN);
    output_t output = parent_output(parent_block, self->key, self->chunk.flags);
    output_chaining_value(&output, new_cv);
    self->cv_stack_len--;
    chunk_counter >>= 1;
  }
  memcpy(&self->cv_stack[self->cv_stack_len * BLAKE3_OUT_LEN], new_cv,
         BLAKE3_OUT_LEN);
  self->cv_stack_len++;
}

void blake3_hasher_update(blake3_hasher *self, const void *input,
                          size_t input_len) {
  const uint8_t *input_bytes = (const uint8_t *)input;
  while (input_len > 0) {
    if (chunk_state_len(&self->chunk) == BLAKE3_CHUNK_LEN) {
      output_t output = chunk_state_output(&self->chunk);
      uint8_t chunk_cv[32];
      output_chaining_value(&output, chunk_cv);
      uint64_t total_chunks = self->chunk.chunk_counter + 1;
      hasher_push_cv(self, chunk_cv, total_chunks);
      chunk_state_reset(&self->chunk, self->key, total_chunks);
    }

    size_t want = BLAKE3_CHUNK_LEN - chunk_state_len(&self->chunk);
    size_t take = want;
    if (take > input_len) {
      take = input_len;
    }
    chunk_state_update(&self->chunk, input_bytes, take);
    input_bytes += take;
    input_len -= take;
  }
}

void blake3_hasher_finalize_seek(const blake3_hasher *self, uint64_t seek,
                                 void *out, size_t out_len) {
  output_t output = chunk_state_output(&self->chunk);
  size_t cvs_remaining = self->cv_stack_len;
  while (cvs_remaining > 0) {
    uint8_t parent_block[BLAKE3_BLOCK_LEN];
    memcpy(parent_block, &self->cv_stack[(cvs_remaining - 1) * BLAKE3_OUT_LEN],
           BLAKE3_OUT_LEN);
    output_chaining_value(&output, parent_block + BLAKE3_OUT_LEN);
    output = parent_output(parent_block, self->key, self->chunk.flags);
    cvs_remaining--;
  }
  output_root_bytes(&output, seek, (uint8_t *)out, out_len);
}

void blake3_hasher_finalize(const blake3_hasher *self, void *out,
                            size_t out_len) {
  blake3_hasher_finalize_seek(self, 0, out, out_len);
}
