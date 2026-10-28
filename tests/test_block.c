/*
An implementation of Delta/Huffman Compressor (DHC) for embedded systems.

Copyright (c) 2022
Marcelo Barros de Almeida <marcelobarrosalmeida@gmail.com>
Marden Fagundes <maf_cadastro-github@yahoo.com>
*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dhc.h"

static bool samples_match(int16_t *a, int16_t *b, uint32_t n)
{
    for(uint32_t pos = 0 ; pos < n ; pos++)
    {
        if(a[pos] != b[pos])
        {
            printf("  mismatch at sample %u: %d != %d\n",pos,a[pos],b[pos]);
            return false;
        }
    }

    return true;
}

static void gen_data(int16_t *data, uint32_t size, int16_t min, int16_t max)
{
    for(uint32_t pos = 0 ; pos < size ; pos++)
        data[pos] = (rand() % ((max-min)+1)) + min;
}

// compresses `samples` split into blocks given by `block_sizes`/`n_blocks`, with generous
// per-call capacity (so no partial consumption happens here), appends the resulting bytes
// into `out`, returns the total compressed size in bits
static uint32_t block_compress_all(uint8_t *out, int16_t *samples, uint32_t *block_sizes, uint32_t n_blocks, uint8_t *map)
{
    dhc_ctx_t ctx;
    uint32_t sample_pos = 0;
    uint32_t out_byte_pos = 0;
    uint32_t total_bits = 0;

    dhc_ctx_init(&ctx,DHC_CTX_COMPRESS,map);

    for(uint32_t b = 0 ; b < n_blocks ; b++)
    {
        uint32_t sample_size = block_sizes[b];
        uint32_t data_size_bits = block_sizes[b]*32; // generous worst case (30 bits/sample max)
        bool last_block = (b == n_blocks-1);

        bool ok = dhc_block_compress(&ctx,out+out_byte_pos,&data_size_bits,samples+sample_pos,&sample_size,last_block);

        if((!ok)||(sample_size != block_sizes[b]))
        {
            printf("  block_compress failed at block %u\n",b);
            return 0;
        }

        sample_pos += sample_size;
        out_byte_pos += data_size_bits/8;
        total_bits += data_size_bits;
    }

    // last block may end on a non byte boundary; total_bits already accounts for that
    // (data_size_bits for the last block is the exact bit count, not rounded)
    return total_bits;
}

// decompresses `data`/`data_size_bits` fed in fixed-size byte chunks (chunk_bytes), unrelated
// to the block boundaries used during compression, to prove decoding does not depend on them
static uint32_t block_decompress_all(int16_t *out, uint32_t out_capacity, uint8_t *data, uint32_t data_size_bits,
                                      uint32_t chunk_bytes, uint8_t *map)
{
    dhc_ctx_t ctx;
    uint32_t bit_pos = 0;
    uint32_t decoded = 0;

    dhc_ctx_init(&ctx,DHC_CTX_DECOMPRESS,map);

    while(bit_pos < data_size_bits)
    {
        uint32_t remaining_bits = data_size_bits - bit_pos;
        uint32_t chunk_bits = chunk_bytes*8;
        bool last_block;
        uint32_t sample_size;
        uint32_t this_data_size_bits;

        if(chunk_bits >= remaining_bits)
        {
            chunk_bits = remaining_bits;
            last_block = true;
        }
        else
            last_block = false;

        this_data_size_bits = chunk_bits;
        sample_size = out_capacity - decoded;

        bool ok = dhc_block_decompress(&ctx,out+decoded,&sample_size,data+bit_pos/8,&this_data_size_bits,last_block);

        if(!ok)
        {
            printf("  block_decompress failed\n");
            return 0;
        }

        decoded += sample_size;
        bit_pos += this_data_size_bits;
    }

    return decoded;
}

static bool test_roundtrip(const char *name, int16_t *samples, uint32_t sample_count, uint8_t *map,
                            uint32_t *block_sizes, uint32_t n_blocks, uint32_t decode_chunk_bytes)
{
    uint8_t *ref_data = calloc(sample_count,8);
    uint8_t *blk_data = calloc(sample_count,8);
    int16_t *decoded = calloc(sample_count,sizeof(int16_t));
    uint32_t ref_bits;
    uint32_t blk_bits;
    bool pass = true;

    dhc_compress(ref_data,&ref_bits,samples,sample_count,map);

    blk_bits = block_compress_all(blk_data,samples,block_sizes,n_blocks,map);

    if(blk_bits != ref_bits)
    {
        printf("[%s] FAIL: bit size mismatch (block=%u, ref=%u)\n",name,blk_bits,ref_bits);
        pass = false;
    }
    else if(memcmp(ref_data,blk_data,(ref_bits+7)/8) != 0)
    {
        printf("[%s] FAIL: compressed bytes differ from reference\n",name);
        pass = false;
    }

    uint32_t decoded_count = block_decompress_all(decoded,sample_count,blk_data,blk_bits,decode_chunk_bytes,map);

    if(decoded_count != sample_count)
    {
        printf("[%s] FAIL: decoded count %u != %u\n",name,decoded_count,sample_count);
        pass = false;
    }
    else if(!samples_match(samples,decoded,sample_count))
    {
        printf("[%s] FAIL: decoded samples differ\n",name);
        pass = false;
    }

    if(pass)
        printf("[%s] OK (bits=%u)\n",name,blk_bits);

    free(ref_data);
    free(blk_data);
    free(decoded);

    return pass;
}

// verifies that a tiny output buffer makes dhc_block_compress report a partial
// samples_consumed instead of overflowing, and that calling again finishes the job
static bool test_partial_compress(void)
{
    int16_t samples[50];
    uint8_t out[8];
    dhc_ctx_t ctx;
    uint32_t sample_size;
    uint32_t data_size_bits;
    uint32_t total_consumed = 0;
    bool pass = true;

    gen_data(samples,50,-20,20);
    dhc_ctx_init(&ctx,DHC_CTX_COMPRESS,0);

    while(total_consumed < 50)
    {
        sample_size = 50 - total_consumed;
        data_size_bits = sizeof(out)*8;

        bool last_block = false;

        bool ok = dhc_block_compress(&ctx,out,&data_size_bits,samples+total_consumed,&sample_size,last_block);

        if(!ok)
        {
            printf("[partial_compress] FAIL: call returned false\n");
            return false;
        }

        if(sample_size == 0)
        {
            printf("[partial_compress] FAIL: made no progress with an 8 byte buffer\n");
            return false;
        }

        total_consumed += sample_size;
    }

    // flush whatever is left using a last_block call with an empty sample batch
    sample_size = 0;
    data_size_bits = sizeof(out)*8;
    dhc_block_compress(&ctx,out,&data_size_bits,samples,&sample_size,true);

    printf("[partial_compress] OK (consumed all %u samples across small 8 byte buffers)\n",total_consumed);

    return pass;
}

// verifies that a tiny sample buffer makes dhc_block_decompress report a partial
// data_bits_consumed (via data_size_bits) instead of overflowing `samples`
static bool test_partial_decompress(void)
{
    int16_t samples[50];
    uint8_t compressed[400];
    int16_t decoded[50];
    uint32_t compressed_bits;
    dhc_ctx_t ctx;
    uint32_t total_decoded = 0;
    bool pass = true;

    gen_data(samples,50,-20,20);
    dhc_compress(compressed,&compressed_bits,samples,50,0);

    dhc_ctx_init(&ctx,DHC_CTX_DECOMPRESS,0);

    uint32_t bit_pos = 0;

    while(total_decoded < 50)
    {
        uint32_t sample_size = 3; // tiny output capacity, smaller than what remains to decode
        uint32_t data_size_bits = compressed_bits - bit_pos;

        bool ok = dhc_block_decompress(&ctx,decoded+total_decoded,&sample_size,compressed+bit_pos/8,&data_size_bits,true);

        if(!ok)
        {
            printf("[partial_decompress] FAIL: call returned false\n");
            return false;
        }

        if(sample_size == 0)
        {
            printf("[partial_decompress] FAIL: made no progress\n");
            return false;
        }

        total_decoded += sample_size;
        bit_pos += data_size_bits;
    }

    if(!samples_match(samples,decoded,50))
    {
        printf("[partial_decompress] FAIL: decoded samples differ\n");
        pass = false;
    }
    else
        printf("[partial_decompress] OK (decoded all 50 samples 3 at a time)\n");

    return pass;
}

// verifies dhc_file_ctx_init()/dhc_file_block_write() produce the exact same file as writing
// the whole compressed buffer at once with dhc_file_write()
static bool test_file_block_write(void)
{
    int16_t samples[200];
    uint8_t ref_compressed[800];
    uint8_t blk_compressed[800];
    uint32_t ref_bits;
    dhc_ctx_t ctx;
    dhc_file_ctx_t file_ctx;
    uint32_t bs[] = {7,13,29,41,53,57};
    uint32_t n = sizeof(bs)/sizeof(bs[0]);
    uint32_t pos = 0;
    bool pass = true;
    FILE *f1;
    FILE *f2;
    int c1;
    int c2;

    gen_data(samples,200,-30,30);
    dhc_compress(ref_compressed,&ref_bits,samples,200,0);
    dhc_file_write("test_file_write_ref.bin",ref_compressed,ref_bits,DHC_COMPRESS_MODE_NONE);

    dhc_ctx_init(&ctx,DHC_CTX_COMPRESS,0);
    dhc_file_ctx_init(&file_ctx,"test_file_block_write.bin",DHC_COMPRESS_MODE_NONE);

    for(uint32_t b = 0 ; b < n ; b++)
    {
        uint32_t sample_size = bs[b];
        uint32_t data_size_bits = bs[b]*32; // generous worst case (30 bits/sample max)
        bool last_block = (b == n-1);

        dhc_block_compress(&ctx,blk_compressed,&data_size_bits,samples+pos,&sample_size,last_block);
        dhc_file_block_write(&file_ctx,blk_compressed,data_size_bits,last_block);

        pos += sample_size;
    }

    f1 = fopen("test_file_write_ref.bin","rb");
    f2 = fopen("test_file_block_write.bin","rb");

    if((f1 == 0)||(f2 == 0))
    {
        printf("[file_block_write] FAIL: could not open output files\n");
        pass = false;
    }
    else
    {
        do
        {
            c1 = fgetc(f1);
            c2 = fgetc(f2);

            if(c1 != c2)
            {
                printf("[file_block_write] FAIL: files differ\n");
                pass = false;
                break;
            }
        } while(c1 != EOF);
    }

    if(f1)
        fclose(f1);

    if(f2)
        fclose(f2);

    remove("test_file_write_ref.bin");
    remove("test_file_block_write.bin");

    if(pass)
        printf("[file_block_write] OK (matches dhc_file_write byte for byte)\n");

    return pass;
}

int main(void)
{
    srand(42);

    uint32_t sample_count = 2000;
    int16_t *samples = calloc(sample_count,sizeof(int16_t));
    uint8_t map[DHC_TABLE_SIZE];
    uint32_t dummy_bits;

    gen_data(samples,sample_count,-100,100);

    bool all_pass = true;

    // block sizes of 1 (worst case for crossing symbol boundaries all the time)
    {
        uint32_t *bs = calloc(sample_count,sizeof(uint32_t));
        for(uint32_t i = 0 ; i < sample_count ; i++)
            bs[i] = 1;
        all_pass &= test_roundtrip("blocks of 1 sample, decode chunks of 1 byte",samples,sample_count,0,bs,sample_count,1);
        free(bs);
    }

    // irregular block sizes, no custom map
    {
        uint32_t bs[] = {1,2,3,5,7,11,13,17,19,23,29,37,41,53,97,131,197,311,499,733};
        uint32_t n = sizeof(bs)/sizeof(bs[0]);
        uint32_t sum = 0;
        for(uint32_t i = 0 ; i < n ; i++)
            sum += bs[i];
        bs[n-1] += (sample_count - sum);
        all_pass &= test_roundtrip("irregular blocks, no map, decode chunks of 3 bytes",samples,sample_count,0,bs,n,3);
    }

    // irregular block sizes, custom map
    {
        dhc_compress_evaluate(samples,sample_count,&dummy_bits,map);
        uint32_t bs[] = {1,2,3,5,7,11,13,17,19,23,29,37,41,53,97,131,197,311,499,733};
        uint32_t n = sizeof(bs)/sizeof(bs[0]);
        uint32_t sum = 0;
        for(uint32_t i = 0 ; i < n ; i++)
            sum += bs[i];
        bs[n-1] += (sample_count - sum);
        all_pass &= test_roundtrip("irregular blocks, custom map, decode chunks of 5 bytes",samples,sample_count,map,bs,n,5);
    }

    // single block (equivalent to the whole-buffer API)
    {
        uint32_t bs[] = { sample_count };
        all_pass &= test_roundtrip("single block, no map, decode in one chunk",samples,sample_count,0,bs,1,sample_count*8);
    }

    all_pass &= test_partial_compress();
    all_pass &= test_partial_decompress();
    all_pass &= test_file_block_write();

    printf(all_pass ? "\nALL TESTS PASSED\n" : "\nSOME TESTS FAILED\n");

    free(samples);

    return all_pass ? 0 : 1;
}
