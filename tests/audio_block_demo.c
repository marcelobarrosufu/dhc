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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "dhc.h"
#include "audio.h"

#define APP_USE_CUSTOM_MAPPING 1
#define APP_DATA_SIZE 5000
#define APP_BLOCK_SIZE 128
#define APP_NUMBER_DRAW(a,b) ((rand() % ((b-a)+1)) + (a))

uint8_t compressed[AUDIO_SAMPLE_SIZE*2];
int16_t decode[AUDIO_SAMPLE_SIZE];

void app_gen_data(int16_t *data, uint32_t size, int16_t min, int16_t max)
{
    for(size_t pos = 0 ; pos < size ; pos++)
        data[pos] = APP_NUMBER_DRAW(min,max);
}

bool verify(int16_t *dec, int16_t *enc, uint32_t size)
{
    bool match = true;
    for(size_t pos = 0 ; pos < size ; pos++)
    {
        if(dec[pos] != enc[pos])
        {
            printf("Invalid byte pos %lu (%d <> %d)\r\n",pos,dec[pos],enc[pos]);
            match = false;
            break;
        }
    }

    return match;
}

void audio_block_demo(void)
{
#if APP_USE_CUSTOM_MAPPING == 1
    uint8_t map[DHC_TABLE_SIZE];
#else
    uint8_t *map = 0;
#endif
    uint32_t compressed_size_bits;
    uint32_t decode_size;
    uint32_t encode_size = AUDIO_SAMPLE_SIZE;
    dhc_ctx_t ctx;
    uint32_t pos;

    float comp_rate = dhc_compress_evaluate(audio_var,encode_size,&compressed_size_bits,map);

    if(comp_rate <= 0)
    {
        printf("Compressor will not reduce the data (%2.2f%%)\r\n",comp_rate);
        return;
    }

    printf("Compressor rate (%2.2f%%)\r\n",comp_rate);

    // compress audio_var as if it arrived incrementally, APP_BLOCK_SIZE samples at a time
    dhc_ctx_init(&ctx,DHC_CTX_DIR_COMPRESS,map);
    compressed_size_bits = 0;
    pos = 0;

    while(pos < encode_size)
    {
        uint32_t sample_size = encode_size - pos < APP_BLOCK_SIZE ? encode_size - pos : APP_BLOCK_SIZE;
        uint32_t data_size_bits = sizeof(compressed)*8 - compressed_size_bits;
        bool last_block = (pos + sample_size) >= encode_size;

        dhc_block_compress(&ctx,compressed + compressed_size_bits/8,&data_size_bits,audio_var + pos,&sample_size,last_block);

        pos += sample_size;
        compressed_size_bits += data_size_bits;
    }

    // decompress the resulting stream in chunks of APP_BLOCK_SIZE bytes, unrelated to the
    // block sizes used above, to show decoding does not need to match them
    dhc_ctx_init(&ctx,DHC_CTX_DIR_DECOMPRESS,map);
    decode_size = 0;
    pos = 0;

    while(pos < compressed_size_bits)
    {
        uint32_t remain_bits = compressed_size_bits - pos;
        uint32_t data_size_bits = remain_bits < APP_BLOCK_SIZE*8 ? remain_bits : APP_BLOCK_SIZE*8;
        uint32_t sample_size = AUDIO_SAMPLE_SIZE - decode_size;
        bool last_block = (pos + data_size_bits) >= compressed_size_bits;

        dhc_block_decompress(&ctx,decode + decode_size,&sample_size,compressed + pos/8,&data_size_bits,last_block);

        decode_size += sample_size;
        pos += data_size_bits;
    }

    bool match = verify(decode,audio_var,decode_size);
    printf("Result: %s\r\n",match?"OK":"ERROR");
}

int main(void)
{
    srand(time(NULL));
    audio_block_demo();

    return 0;
}
