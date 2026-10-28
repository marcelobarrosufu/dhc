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
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include "dhc.h"

#define APP_NUMBER_DRAW(a, b) ((rand() % ((b - a) + 1)) + (a))

extern char *optarg;
extern int optind, opterr, optopt;

void app_gen_data(int16_t *data, uint32_t size, int16_t min, int16_t max)
{
    for (size_t pos = 0; pos < size; pos++)
        data[pos] = APP_NUMBER_DRAW(min, max);
}

bool verify(int16_t *dec, int16_t *enc, uint32_t size)
{
    bool match = true;
    for (size_t pos = 0; pos < size; pos++)
    {
        if (dec[pos] != enc[pos])
        {
            printf("Invalid byte pos %lu (%d <> %d)\r\n", pos, dec[pos], enc[pos]);
            match = false;
            break;
        }
    }

    return match;
}

bool run_test(uint32_t frame_size, int16_t min_value, int16_t max_value, bool use_alt_map, float *rate, bool *mapped)
{
    bool match = true;
    uint8_t *pmap;
    uint8_t map[DHC_TABLE_SIZE];
    uint32_t data_decoded_size;
    uint32_t data_encoded_size_bits;
    uint32_t data_original_size = frame_size;
    int16_t *data_original;
    uint8_t *data_encoded;
    int16_t *data_decoded;
    uint32_t data_encoded_size_in_bits;

    data_original = (int16_t *)calloc(frame_size, sizeof(int16_t));
    data_encoded = (uint8_t *)calloc(frame_size * sizeof(int16_t), 1);
    data_decoded = (int16_t *)calloc(frame_size, sizeof(int16_t));

    assert(data_original);
    assert(data_encoded);
    assert(data_decoded);

    app_gen_data(data_original, data_original_size, min_value, max_value);

    float rate_reg = dhc_compress_evaluate(data_original, data_original_size, &data_encoded_size_in_bits, 0);
    float rate_map = dhc_compress_evaluate(data_original, data_original_size, &data_encoded_size_in_bits, map);

    if ((rate_reg <= rate_map) && use_alt_map)
    {
        pmap = map;
        *rate = rate_map;
        *mapped = true;
    }
    else
    {
        pmap = 0;
        *rate = rate_reg;
        *mapped = false;
    }

    if (*rate > 0)
    {
        dhc_compress(data_encoded, &data_encoded_size_bits, data_original, data_original_size, pmap);
        dhc_decompress(data_decoded, &data_decoded_size, data_encoded, data_encoded_size_bits, pmap);
        match = verify(data_decoded, data_original, data_decoded_size);
    }

    free(data_original);
    free(data_encoded);
    free(data_decoded);

    return match;
}

void test_loop(uint32_t repetitions, uint32_t frame_size, int16_t min_value, int16_t max_value, bool use_alt_map, bool verbose)
{
    double rate_sum = 0.0;
    float rate = 0;
    uint32_t samples = 0;
    uint32_t skipped = 0;
    uint32_t mapped_samples = 0;
    bool mapped;

    for (size_t num = 0; num < repetitions; num++)
    {
        if (run_test(frame_size, min_value, max_value, use_alt_map, &rate, &mapped) == false)
        {
            if (verbose)
                printf("Encoder error!!!\r\n");
            break;
        }

        if (rate > 0)
        {
            rate_sum += rate;

            samples++;
            if (mapped)
                mapped_samples++;
        }
        else
            skipped++;

        if (verbose)
        {
            printf("Running test %lu/%d reduction=%2.2f%% skipped=%d mapped=%d\r\n",
                   num + 1,
                   repetitions,
                   samples == 0 ? 0.0 : rate_sum / samples,
                   skipped,
                   mapped_samples);
        }
    }

    if(verbose)
        printf("Fields: Delta,Compress_reduction,skipped,skipped_per,mapped,mapped_perc\r\n");
    
    printf("%d,%2.2f%%,%d,%2.2f%%,%d,%2.2f%%\r\n",
           (max_value-min_value),
           samples == 0 ? 0.0 : rate_sum / samples,
           skipped,
           repetitions == 0 ? 0.0 : 100.0 * skipped / repetitions,
           mapped_samples,
           repetitions == 0 ? 0.0 : 100.0 * mapped_samples / repetitions);
}

void help(char *app)
{
    printf("Usage (no custom dictionary):\r\n");
    printf("%s -m min_value -M max_value -f frame_size -r repetitions -v\r\n", app);
    printf("Usage (checking custom dictionary):\r\n");
    printf("%s -m min_value -M max_value -f frame_size -r repetitions -a -v\r\n", app);
}

int main(int argc, char **argv)
{
    int option;
    int16_t min_value = INT16_MIN;
    int16_t max_value = INT16_MAX;
    uint32_t frame_size = 1024;
    uint32_t repetitions = 1000;
    bool verbose = false;
    bool parsed = true;
    bool use_alt_map = false;

    srand(time(NULL));

    while ((option = getopt(argc, argv, ":m:M:f:r:hva")) != -1)
    {
        switch (option)
        {
        case 'v':
            verbose = true;
            break;
        case 'a':
            use_alt_map = true;
            break;
        case 'm':
            sscanf(optarg, "%hd", &min_value);
            break;
        case 'M':
            sscanf(optarg, "%hd", &max_value);
            break;
        case 'f':
            sscanf(optarg, "%d", &frame_size);
            break;
        case 'r':
            sscanf(optarg, "%d", &repetitions);
            break;
        case 'h':
            parsed = false;
            break;
        case ':':
            printf("Missing argument for -%c\n", optopt);
            parsed = false;
            break;
        }
    }

    if (!parsed)
    {
        help(argv[0]);
        return -1;
    }

    if (verbose)
    {
        printf("Starting %d repetitions (frame size: %d, interval: [%d,%d])\r\n",
               repetitions, frame_size, min_value, max_value);
    }

    test_loop(repetitions, frame_size, min_value, max_value, use_alt_map, verbose);

    return 0;
}
