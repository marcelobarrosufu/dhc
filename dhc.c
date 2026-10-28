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
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#include "dhc.h"

#define DHC_DEBUG 0
#define DHC_ABS(x)     (((x) < 0) ? -(x) : (x))

// worst case for a single symbol: num_bits_prefix(14) + num_bits_index(16), see dhc_table[16]
#define DHC_MAX_SYMBOL_BITS 30

typedef struct dhc_stream_ctrl_s
{
    uint8_t *stream;
    uint16_t byte_pos;
    uint8_t bit_pos;
    uint32_t size_bits;
    uint64_t bit_acc;
    uint8_t bit_acc_bits;
} dhc_stream_ctrl_t;

struct dhc_table_s
{
    uint16_t num_bits_prefix;
    uint16_t num_bits_index;
    uint32_t value_prefix;
    uint8_t *str_prefix;
};

static void dhc_io_put8_tb(uint8_t value, uint8_t *buf)
{
    buf[0] = value;
}

static void dhc_io_put8_tb_apr(uint8_t value, uint8_t **buf)
{
    dhc_io_put8_tb(value,*buf);
    *buf += 1;
}

static void dhc_io_put32_tb(uint32_t value, uint8_t *buf)
{
    buf[3] = (uint8_t)(value      );
    buf[2] = (uint8_t)(value >> 8 );
    buf[1] = (uint8_t)(value >> 16);
    buf[0] = (uint8_t)(value >> 24);
}

static void dhc_io_put32_tb_apr(uint32_t value, uint8_t **buf)
{
    dhc_io_put32_tb(value,*buf);
    *buf += 4;
}

#define dhc_io_put8_tb_ap(v,x)  dhc_io_put8_tb_apr(v,&x)
#define dhc_io_put32_tb_ap(v,x) dhc_io_put32_tb_apr(v,&x)

// prefix bits, index bits, prefix value, prefix string
struct dhc_table_s dhc_table[] = {
    /*  0 */ { 2,  0, 0x00000000, (uint8_t *) "00"},
    /*  1 */ { 3,  1, 0x00000002, (uint8_t *) "010"},
    /*  2 */ { 3,  2, 0x00000006, (uint8_t *) "011"},
    /*  3 */ { 3,  3, 0x00000001, (uint8_t *) "100"},
    /*  4 */ { 3,  4, 0x00000005, (uint8_t *) "101"},
    /*  5 */ { 3,  5, 0x00000003, (uint8_t *) "110"},
    /*  6 */ { 4,  6, 0x00000007, (uint8_t *) "1110"},
    /*  7 */ { 5,  7, 0x0000000F, (uint8_t *) "11110"},
    /*  8 */ { 6,  8, 0x0000001F, (uint8_t *) "111110"},
    /*  9 */ { 7,  9, 0x0000003F, (uint8_t *) "1111110"},
    /* 10 */ { 8, 10, 0x0000007F, (uint8_t *) "11111110"},
    /* 11 */ { 9, 11, 0x000000FF, (uint8_t *) "111111110"},
    /* 12 */ {10, 12, 0x000001FF, (uint8_t *) "1111111110"},
    /* 13 */ {11, 13, 0x000003FF, (uint8_t *) "11111111110"},
    /* 14 */ {12, 14, 0x000007FF, (uint8_t *) "111111111110"},
    /* 15 */ {13, 15, 0x00000FFF, (uint8_t *) "1111111111110"},
    /* 16 */ {14, 16, 0x00001FFF, (uint8_t *) "11111111111110"},
    };

static uint8_t dhc_log2_down_abs(int16_t _value)
{
    uint8_t res = 0;
    uint16_t value = (uint16_t) DHC_ABS(_value);

    if(value)
    {
        res = -1;
        while(value)
        {
            value = value >> 1;
            res++;
        }
    }

    return res;
}

static uint8_t dhc_log2_up_abs(int16_t _value)
{
    uint8_t res = 0;
    uint16_t value = (uint16_t) DHC_ABS(_value);

    if(value)
    {
        while(value)
        {
            value = value >> 1;
            res++;
        }
    }

    return res;
}

static uint32_t dhc_pow2(uint8_t value)
{
    uint32_t pow = 1;

    if(value)
    {
        while(value)
        {
            value--;
            pow = pow*2;
        }
    }

    return pow;
}

static void dhc_hist_reset(uint32_t *hist, uint8_t *map)
{
    for(size_t pos = 0 ; pos < DHC_TABLE_SIZE ; pos++)
    {
        hist[pos] = 0;
        map[pos] = pos;
    }
}
static void dhc_hist_update(uint32_t *hist, uint8_t bin)
{
    hist[bin]++;
}

static void dhc_hist_print(uint32_t *hist, uint8_t *map)
{
#if DHC_DEBUG == 1
    printf("HISTOGRAM:\r\n");

    bool use_map = map == 0 ? false : true;

    for(size_t bin = 0 ; bin < DHC_TABLE_SIZE ; bin++)
    {
        if(use_map)
            printf("[%02u][%05u] %u (±%u±%u)\r\n",map[bin],dhc_pow2(map[bin]),hist[bin],map[bin]>0?dhc_pow2(map[bin]-1):0,dhc_pow2(map[bin])-1);
        else
            printf("[%02lu][%05u] %u (±%u±%u)\r\n",bin,dhc_pow2(bin),hist[bin],bin>0?dhc_pow2(bin-1):0,dhc_pow2(bin)-1);
    }
#endif
}

static void dhc_hist_bubble_sort(uint32_t *hist, uint8_t *idx, size_t size)
{
    size_t i, j;

    for (i = 0; i < size-1; i++)
    {
        for (j = 0; j < size-i-1; j++)
        {
            if (hist[j] < hist[j+1])
            {
                uint32_t t = hist[j];
                hist[j] = hist[j+1];
                hist[j+1] = t;

                uint8_t v = idx[j];
                idx[j] = idx[j+1];
                idx[j+1] = v;
            }
        }
    }
}

#if DHC_DEBUG == 1
static void dhc_prefix_print(uint16_t bin)
{
    printf("%s",dhc_table[bin].str_prefix);
}

static void dhc_index_print(uint16_t value, uint16_t num_bits)
{
    for(size_t pos = num_bits ; pos > 0 ; pos--)
    {
        printf("%c",(value >> (pos-1)) & 0x01 ? '1' : '0');
    }
}
#endif

static void dhc_stream_init(dhc_stream_ctrl_t *ctrl, uint8_t *stream, uint16_t size, bool compress)
{
    ctrl->stream = stream;
    ctrl->bit_pos = 0;
    ctrl->byte_pos = 0;
    ctrl->bit_acc = 0;
    ctrl->bit_acc_bits = 0;

    if(compress)
        ctrl->stream[0] = 0;
}

static uint32_t dhc_stream_size_bits_get(dhc_stream_ctrl_t *ctrl)
{
    return ctrl->byte_pos*8 + ctrl->bit_pos;
}

// bits still available to read in ctrl: whatever is pending in bit_acc (carried over from a
// previous block) plus whatever is still unread in stream, up to size_bits
static uint32_t dhc_stream_avail_get(dhc_stream_ctrl_t *ctrl)
{
    return ctrl->bit_acc_bits + (ctrl->size_bits - dhc_stream_size_bits_get(ctrl));
}

static void dhc_stream_add(dhc_stream_ctrl_t *ctrl, uint32_t bit_stream, uint8_t size)
{
    uint8_t remain = size;

    if(size == 0)
        return;

    ctrl->stream[ctrl->byte_pos] |= (uint8_t)(bit_stream << ctrl->bit_pos);

    if(ctrl->bit_pos + size >= 8)
    {
        remain -= (8-ctrl->bit_pos);
        bit_stream >>= (8-ctrl->bit_pos);
        ctrl->bit_pos = 0;
        ctrl->byte_pos++;
        ctrl->stream[ctrl->byte_pos] = 0;
    }
    else
    {
        ctrl->bit_pos += size;
        remain -= size;
    }

    while(remain)
    {
        ctrl->stream[ctrl->byte_pos] = (uint8_t)(bit_stream);

        if(remain < 8)
        {
            ctrl->bit_pos = remain;
            remain = 0;
        }
        else
        {
            bit_stream >>= 8;
            remain -= 8;
            ctrl->byte_pos++;
            ctrl->stream[ctrl->byte_pos] = 0;
        }
    }
}

// same as the original byte-oriented reader, except bits pending in ctrl->bit_acc (carried
// over from a previous block) are consumed first, before touching ctrl->stream at all
static uint32_t dhc_stream_next_bits_get(dhc_stream_ctrl_t *ctrl, uint8_t size)
{
    uint8_t remain = size;
    uint32_t ofs = 0;
    uint32_t bit_stream = 0;
    uint8_t take;
    uint8_t bit;

    if(size == 0)
        return 0;

    if(ctrl->bit_acc_bits > 0)
    {
        take = ctrl->bit_acc_bits < remain ? ctrl->bit_acc_bits : remain;
        bit_stream = (uint32_t)(ctrl->bit_acc & (((uint64_t)1 << take) - 1));
        ctrl->bit_acc >>= take;
        ctrl->bit_acc_bits -= take;
        remain -= take;
        ofs = take;
    }

    while(remain)
    {
        bit = (ctrl->stream[ctrl->byte_pos] >> ctrl->bit_pos) & 0x01;
        bit_stream |= ((uint32_t) bit) << ofs;

        if(++ctrl->bit_pos >= 8)
        {
            ctrl->bit_pos = 0;
            ctrl->byte_pos++;
        }

        ofs++;
        remain--;
    }

    return bit_stream;
}

static uint32_t dhc_stream_prefix_end_get(dhc_stream_ctrl_t *ctrl)
{
    uint32_t token = 0;
    uint32_t ofs = 0;
    uint8_t bit;

    do
    {
        bit = (uint8_t) dhc_stream_next_bits_get(ctrl,1);
        token |= (bit << ofs);
        ofs++;

    } while(bit);

    return token;
}

static int16_t dhc_index2value(int16_t index, uint8_t bin)
{
    int16_t value;
    uint32_t pwr_bin = dhc_pow2(bin);

    if(index < (pwr_bin/2))
        value = 1 + index - pwr_bin;
    else
        value = index;

    return value;
}

static int16_t dhc_stream_next_value_get(dhc_stream_ctrl_t *ctrl, uint8_t *map)
{
    int16_t index = 0;
    int16_t data = 0;
    uint32_t bit_stream;
    uint8_t bin = 0;
    uint8_t map_bin;

    bit_stream = dhc_stream_next_bits_get(ctrl,dhc_table[0].num_bits_prefix);

    if(bit_stream == dhc_table[0].value_prefix) // 00
    {
        bin = 0;
    }
    else
    {
        bit_stream |= (dhc_stream_next_bits_get(ctrl,1) << 2);

        for(bin = 1 ; bin < 6 ; bin++)
        {
            if(bit_stream == dhc_table[bin].value_prefix)
                break;
        }

        if(bin >= 6)
        {
            // 110, 1110, 11110 ...
            // 011, 0111, 01111 ...
            // 3, 7, F, 1F, 3F ...
            // +1: 4, 8, 16, 32, 64 ...
            // log2(): 2, 3, 4, 5, ...
            // +3: 5, 6, 7, 8 ...
            bit_stream |= (dhc_stream_prefix_end_get(ctrl) << 3);
            bin = dhc_log2_down_abs(bit_stream + 1) + 3;
        }
    }

    map_bin = map ? map[bin] : bin;

#if DHC_DEBUG == 1
    dhc_prefix_print(bin);
    printf(" ");
    fflush(stdout);
#endif

    index = dhc_stream_next_bits_get(ctrl,dhc_table[map_bin].num_bits_index);
    data = dhc_index2value(index,map_bin);

#if DHC_DEBUG == 1
    dhc_index_print(index,dhc_table[map_bin].num_bits_index);
    printf(" (%d -> %d) (%d -> %d)\r\n",data,index,bin,map_bin);
    fflush(stdout);
#endif

    return data;
}

bool dhc_ctx_init(dhc_ctx_t *ctx, dhc_ctx_dir_t dir, uint8_t *map)
{
    if(ctx == 0)
        return false;

    ctx->dir = dir;
    ctx->prev_value = 0;
    ctx->map = map;
    ctx->carry_byte = 0;
    ctx->carry_bits = 0;
    ctx->bit_acc = 0;
    ctx->bit_acc_bits = 0;

    if(map)
    {
        for(size_t pos = 0 ; pos < DHC_TABLE_SIZE ; pos++)
            ctx->map_bin[map[pos]] = pos;
    }

    return true;
}

bool dhc_block_compress(dhc_ctx_t *ctx, uint8_t *data, uint32_t *data_size_bits,
                         int16_t *samples, uint32_t *sample_size, bool last_block)
{
    dhc_stream_ctrl_t ctrl =  { 0 };
    uint8_t bin;
    int16_t index;
    int16_t delta;
    uint8_t map_bin;
    uint32_t pos;
    uint32_t total_bits;
    uint32_t full_bytes;
    uint8_t remain_bits;

    if((ctx == 0)||(data == 0)||(data_size_bits == 0)||(samples == 0)||(sample_size == 0))
        return false;

    if(ctx->dir != DHC_CTX_DIR_COMPRESS)
        return false;

    dhc_stream_init(&ctrl,data,0,true);

    if(ctx->carry_bits > 0)
        dhc_stream_add(&ctrl,ctx->carry_byte,ctx->carry_bits);

    for(pos = 0 ; pos < *sample_size ; pos++)
    {
        delta = samples[pos] - ctx->prev_value;
        bin = dhc_log2_up_abs(delta);

        if(delta >= 0)
            index = delta;
        else
            index = dhc_pow2(bin) - 1 - DHC_ABS(delta);

        map_bin = ctx->map ? ctx->map[bin] : bin;

        total_bits = dhc_stream_size_bits_get(&ctrl) + dhc_table[map_bin].num_bits_prefix + dhc_table[bin].num_bits_index;

        if(total_bits > *data_size_bits)
            break;

#if DHC_DEBUG == 1
        dhc_prefix_print(map_bin);
        printf(" ");
        dhc_index_print(index,dhc_table[bin].num_bits_index);
        printf(" (%d -> %d) (%d -> %d)\r\n",delta,index,bin,map_bin);
        fflush(stdout);
#endif

        dhc_stream_add(&ctrl,dhc_table[map_bin].value_prefix,dhc_table[map_bin].num_bits_prefix);
        dhc_stream_add(&ctrl,index,dhc_table[bin].num_bits_index);

        ctx->prev_value = samples[pos];
    }

    *sample_size = pos;

    total_bits = dhc_stream_size_bits_get(&ctrl);

    if(last_block)
    {
        *data_size_bits = total_bits;
        ctx->carry_byte = 0;
        ctx->carry_bits = 0;
    }
    else
    {
        full_bytes = total_bits / 8;
        remain_bits = total_bits % 8;

        *data_size_bits = full_bytes * 8;
        ctx->carry_byte = remain_bits > 0 ? data[full_bytes] : 0;
        ctx->carry_bits = remain_bits;
    }

    return true;
}

bool dhc_compress(uint8_t *output, uint32_t *output_size_bits, int16_t *data, uint32_t size, uint8_t *map)
{
    dhc_ctx_t ctx;
    uint32_t sample_size;

    if((size < 1)||(data == 0)||(output == 0)||(output_size_bits == 0))
        return false;

    dhc_ctx_init(&ctx,DHC_CTX_DIR_COMPRESS,map);

    sample_size = size;
    *output_size_bits = UINT32_MAX;

    return dhc_block_compress(&ctx,output,output_size_bits,data,&sample_size,true);
}

float dhc_compress_evaluate(int16_t *data, uint32_t size, uint32_t *compressed_size_in_bits, uint8_t *map)
{
    uint8_t bin;
    int16_t delta;
    int16_t prev_value;
    uint32_t hist[DHC_TABLE_SIZE];
    uint8_t map_bin[DHC_TABLE_SIZE];
    // int16_t index;

    if((size < 1)||(data == 0))
        return 0.0;

    dhc_hist_reset(hist,map_bin);

    uint32_t total_bits = 0;
    uint32_t compressed_bits = 0;

    // data[-1] => should be central value among all possible discrete values
    // Worth the calc ?
    prev_value = 0;

    for(size_t pos = 0 ; pos < size ; pos++)
    {
        delta = data[pos] - prev_value;
        prev_value = data[pos];
        bin = dhc_log2_up_abs(delta);
        dhc_hist_update(hist,bin);

        total_bits += 16;
        compressed_bits += dhc_table[bin].num_bits_prefix + dhc_table[bin].num_bits_index;
    }

    dhc_hist_print(hist,0);

    float compress_rate = 100.0*((float)total_bits-compressed_bits)/total_bits;
#if DHC_DEBUG == 1
    printf("Default mapping: %d bits -> %d bits (compressed by %2.2f%%)\r\n",total_bits,compressed_bits,compress_rate);
#endif

    if(map)
    {
        // using better mapping based on frequency ---------------------------
        // weighting by number of bits ?
        // for(size_t pos = 0 ; pos < DHC_TABLE_SIZE ; pos++)
        //    hist[pos] *= dhc_table[pos].num_bits_index > 0 ? dhc_table[pos].num_bits_index : 1;

        dhc_hist_bubble_sort(hist,map_bin,DHC_TABLE_SIZE);

        for(size_t pos = 0 ; pos < DHC_TABLE_SIZE ; pos++)
            map[map_bin[pos]] = pos;

        dhc_hist_print(hist,map);

        total_bits = 0;
        compressed_bits = 0;
        prev_value = 0;

        for(size_t pos = 0 ; pos < size ; pos++)
        {
            delta = data[pos] - prev_value;
            prev_value = data[pos];
            bin = dhc_log2_up_abs(delta);

            total_bits += 16;
            compressed_bits += dhc_table[map[bin]].num_bits_prefix + dhc_table[bin].num_bits_index;
        }

        compress_rate = 100.0*((float)total_bits-compressed_bits)/total_bits;
#if DHC_DEBUG == 1
        printf("Sorted mapping: %d bits -> %d bits (compressed by %2.2f%%)\r\n",total_bits,compressed_bits,compress_rate);
#endif
    }

    *compressed_size_in_bits = compressed_bits;

    return compress_rate;
}

bool dhc_block_decompress(dhc_ctx_t *ctx, int16_t *samples, uint32_t *sample_size,
                           uint8_t *data, uint32_t *data_size_bits, bool last_block)
{
    dhc_stream_ctrl_t ctrl =  { 0 };
    uint32_t pos = 0;
    int16_t delta;
    uint8_t *pmap;
    uint32_t avail;
    uint32_t leftover;
    uint32_t tail;
    uint8_t frac;
    uint8_t bit;

    if((ctx == 0)||(samples == 0)||(sample_size == 0)||(data == 0)||(data_size_bits == 0))
        return false;

    if(ctx->dir != DHC_CTX_DIR_DECOMPRESS)
        return false;

    pmap = ctx->map ? ctx->map_bin : 0;

    dhc_stream_init(&ctrl,data,0,false);
    ctrl.size_bits = *data_size_bits;
    ctrl.bit_acc = ctx->bit_acc;
    ctrl.bit_acc_bits = ctx->bit_acc_bits;

    while(pos < *sample_size)
    {
        avail = dhc_stream_avail_get(&ctrl);

        if(avail == 0)
            break;

        if((avail < DHC_MAX_SYMBOL_BITS)&&(!last_block))
            break;

        delta = dhc_stream_next_value_get(&ctrl,pmap);
        samples[pos] = delta + ctx->prev_value;
        ctx->prev_value = samples[pos];
        pos++;
    }

    *sample_size = pos;

    leftover = dhc_stream_avail_get(&ctrl);

    if(leftover <= DHC_MAX_SYMBOL_BITS)
    {
        ctx->bit_acc = 0;
        ctx->bit_acc_bits = 0;

        while(dhc_stream_avail_get(&ctrl) > 0)
        {
            bit = (uint8_t) dhc_stream_next_bits_get(&ctrl,1);
            ctx->bit_acc |= ((uint64_t) bit) << ctx->bit_acc_bits;
            ctx->bit_acc_bits++;
        }

        *data_size_bits = ctrl.size_bits;
    }
    else
    {
        // more than DHC_MAX_SYMBOL_BITS of data is still unconsumed (stopped because samples
        // got full, not because of a lack of bits): close out the current partial byte (if any)
        // into the carry, on top of whatever carry bits were not yet used, so the caller can
        // always resume with data + *data_size_bits/8, with nothing lost or replayed
        frac = ctrl.bit_pos;

        ctx->bit_acc = ctrl.bit_acc;
        ctx->bit_acc_bits = ctrl.bit_acc_bits;

        if(frac > 0)
        {
            tail = dhc_stream_next_bits_get(&ctrl,8-frac);
            ctx->bit_acc |= ((uint64_t) tail) << ctx->bit_acc_bits;
            ctx->bit_acc_bits += (8-frac);
        }

        *data_size_bits = dhc_stream_size_bits_get(&ctrl);
    }

    return true;
}

bool dhc_decompress(int16_t *output, uint32_t *output_size, uint8_t *data, uint32_t data_size_bits, uint8_t *map)
{
    dhc_ctx_t ctx;
    uint32_t data_size_bits_var;

    if((data_size_bits < 2)||(output == 0)||(data == 0))
        return false;

    dhc_ctx_init(&ctx,DHC_CTX_DIR_DECOMPRESS,map);

    data_size_bits_var = data_size_bits;
    *output_size = UINT32_MAX;

    return dhc_block_decompress(&ctx,output,output_size,data,&data_size_bits_var,true);
}

/*
header DHC_COMPRESS_MODE_HUFFMAN_MAPPED

BB: mode (1 byte)
LLLLLLLL: size in bits (4 bytes)
*/
bool dhc_file_write(char *file_name, uint8_t *data, uint32_t size_bits, dhc_compress_mode_t mode)
{
    bool ret = true;
    uint8_t header[5];
    uint8_t *pbuf;
    uint32_t size;

    if(!size_bits || !data || !file_name)
        return false;

    pbuf = header;
    dhc_io_put8_tb_ap(mode,pbuf);
    dhc_io_put32_tb_ap(size_bits,pbuf);

    FILE *fp = fopen(file_name,"w+b");

    if(fp == NULL)
        return false;

    size = 5;

    if(fwrite(header,1,size,fp) == size)
    {
        size = size_bits % 8 == 0 ? size_bits / 8 : size_bits/8 + 1;

        if(fwrite(data,1,size,fp) != size)
            ret = false;
    }
    else
        ret = false;

    fclose(fp);

    return ret;
}
