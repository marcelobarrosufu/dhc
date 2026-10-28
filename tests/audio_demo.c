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

void audio_demo(void)
{
#if APP_USE_CUSTOM_MAPPING == 1
    uint8_t map[DHC_TABLE_SIZE];
#else
    uint8_t *map = 0;
#endif
    uint32_t compressed_size_bits;
    uint32_t decode_size;
    uint32_t encode_size = AUDIO_SAMPLE_SIZE;

    float comp_rate = dhc_compress_evaluate(audio_var,encode_size,&compressed_size_bits,map);

    if(comp_rate > 0)
    {
        printf("Compressor rate (%2.2f%%)\r\n",comp_rate);
        dhc_compress(compressed,&compressed_size_bits,audio_var,encode_size,map);
        dhc_decompress(decode,&decode_size,compressed,compressed_size_bits,map);
        bool match = verify(decode,audio_var,decode_size);
        printf("Result: %s\r\n",match?"OK":"ERROR");
    }
    else
    {
        printf("Compressor will not reduce the data (%2.2f%%)\r\n",comp_rate);
    }
}

int main(void)
{
    srand(time(NULL));
    audio_demo();

    return 0;
}
