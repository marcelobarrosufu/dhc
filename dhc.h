/*
An implementation of Delta/Huffman Compressor (DHC) for embedded systems.

Copyright (c) 2022 
Marcelo Barros de Almeida <marcelobarrosalmeida@gmail.com>
Marden Fagundes <maf_cadastro-github@yahoo.com>
*/

#pragma once

#define DHC_MAX_BITS    16
#define DHC_TABLE_SIZE (DHC_MAX_BITS+1)

typedef enum dhc_compress_mode_e
{
    DHC_COMPRESS_MODE_NONE = 0,
    DHC_COMPRESS_MODE_HUFFMAN_MAPPED,
} dhc_compress_mode_t;

typedef enum dhc_ctx_dir_e
{
    DHC_CTX_DIR_COMPRESS = 0,
    DHC_CTX_DIR_DECOMPRESS,
} dhc_ctx_dir_t;

typedef struct dhc_ctx_s
{
    dhc_ctx_dir_t dir;
    int16_t prev_value;
    uint8_t *map;
    uint8_t map_rev[DHC_TABLE_SIZE];
    uint8_t carry_byte;
    uint8_t carry_bits;
    uint64_t bit_acc;
    uint8_t bit_acc_bits;
} dhc_ctx_t;

bool dhc_file_write(char *file_name, uint8_t *data, uint32_t size_bits, dhc_compress_mode_t mode);

/*
@brief initialize a context used by dhc_block_compress()/dhc_block_decompress(), fixing its direction
@param[out] ctx context to be initialized (allocated by the caller, e.g. on the stack)
@param[in] dir context direction, DHC_CTX_DIR_COMPRESS or DHC_CTX_DIR_DECOMPRESS
@param[in] map huffman mapping (if using any special huffman map), otherwise pass 0. Must remain valid for the whole lifetime of ctx
@return true in case of success or false if the operation fails
*/
bool dhc_ctx_init(dhc_ctx_t *ctx, dhc_ctx_dir_t dir, uint8_t *map);

/*
@brief compress one block of a (possibly larger) stream of samples, keeping state across calls in ctx
@param[in] ctx context initialized with dhc_ctx_init(&ctx, DHC_CTX_DIR_COMPRESS, map)
@param[out] data block output buffer (must be allocated before calling)
@param[in,out] data_size_bits in: capacity of data, in bits; out: bits actually written (never writes beyond the input capacity)
@param[in] samples sample data for this block (must be a vector of int16_t)
@param[in,out] sample_size in: number of samples offered in this block; out: number of samples actually consumed (may be less than offered if data is full)
@param[in] last_block true if this is the last block of the stream (flushes any pending bits into data)
@return true in case of success or false if the operation fails
*/
bool dhc_block_compress(dhc_ctx_t *ctx, uint8_t *data, uint32_t *data_size_bits,
                         int16_t *samples, uint32_t *sample_size, bool last_block);

/*
@brief decompress one block of a (possibly larger) compressed stream, keeping state across calls in ctx
@param[in] ctx context initialized with dhc_ctx_init(&ctx, DHC_CTX_DIR_DECOMPRESS, map)
@param[out] samples output vector where decompressed samples will be storaged (must be allocated before calling)
@param[in,out] sample_size in: capacity of samples, in int16_t samples; out: number of samples actually decoded
@param[in] data compressed data for this block (must be a vector of uint8_t)
@param[in,out] data_size_bits in: number of valid bits available in data for this block; out: number of bits actually consumed (may be less than offered if samples is full)
@param[in] last_block true if this is the last block of the stream
@return true in case of success or false if the operation fails
*/
bool dhc_block_decompress(dhc_ctx_t *ctx, int16_t *samples, uint32_t *sample_size,
                           uint8_t *data, uint32_t *data_size_bits, bool last_block);

/*
@brief Calculate compress ratio of a vector of 16 bits signed integers (int16_t)
@param[in] samples sample data (must be a vector of int16_t)
@param[in] sample_size sample size (it is not size in bytes, it is size in int16_t samples)
@param[out] data_size_bits compressed output size, in bits (must be allocated before calling)
@param[in] map huffman mapping (if using any special huffman map), otherwise pass 0
@return reduction ratio (if <= 0.0 do not use compress, if > 0.0 you can use compress)
*/
float dhc_compress_evaluate(int16_t *samples, uint32_t sample_size, uint32_t *data_size_bits, uint8_t *map);

/*
@brief compress a vector of 16 bits signed integers (int16_t). Special case of dhc_block_compress()
where the whole vector is compressed at once, using an internal context
@param[out] data vector where compressed data will be storaged (must be allocated before calling)
@param[out] data_size_bits compressed output size, in bits (must be allocated before calling)
@param[in] samples sample data (must be a vector of int16_t)
@param[in] sample_size sample size (it is not size in bytes, it is size in int16_t samples)
@param[in] map huffman mapping (if using any special huffman map), otherwise pass 0
@return true in case of success or false if the operation fails
*/
bool dhc_compress(uint8_t *data, uint32_t *data_size_bits, int16_t *samples, uint32_t sample_size, uint8_t *map);

/*
@brief decompress a vector of bytes (uint8_t). Special case of dhc_block_decompress()
where the whole vector is decompressed at once, using an internal context
@param[out] samples output vector where decompressed samples will be storaged (must be allocated before calling, must be a vector of int16_t)
@param[out] sample_size output size, in bytes (must be allocated before calling, it is not size in bytes, it is size in int16_t samples)
@param[in] data compressed data (must be a vector of unt8_t)
@param[in] data_size_bits compressed data size, in bits
@param[in] map huffman mapping (if using any special huffman map), otherwise pass 0
@return true in case of success or false if the operation fails
*/
bool dhc_decompress(int16_t *samples, uint32_t *sample_size, uint8_t *data, uint32_t data_size_bits, uint8_t *map);
