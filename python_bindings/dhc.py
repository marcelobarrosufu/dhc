#!/usr/bin/python3
"""
An implementation of Delta/Huffman Compressor (DHC) for embedded systems.

Copyright (c) 2022 
Marcelo Barros de Almeida <marcelobarrosalmeida@gmail.com>
Marden Fagundes <maf_cadastro-github@yahoo.com>
"""

from ctypes import *
from random import randint

DHC_MAX_BITS   = 16
DHC_TABLE_SIZE = (DHC_MAX_BITS+1)

dhc_lib_file = './dhc_lib.so'
dhc_functions = CDLL(dhc_lib_file)

DHC_CTX_COMPRESS   = 0
DHC_CTX_DECOMPRESS = 1

class dhc_ctx_t(Structure):
    # mirrors struct dhc_ctx_s from dhc.h, field for field, same order
    _fields_ = [
        ('dir', c_int32),
        ('prev_value', c_int16),
        ('map', POINTER(c_uint8)),
        ('map_bin', c_uint8 * DHC_TABLE_SIZE),
        ('carry_byte', c_uint8),
        ('carry_bits', c_uint8),
        ('bit_acc', c_uint64),
        ('bit_acc_bits', c_uint8),
    ]

def ctx_init(ctx,dir,map=[]):
    # bool dhc_ctx_init(dhc_ctx_t *ctx, dhc_ctx_dir_t dir, uint8_t *map);
    dhc_functions.dhc_ctx_init.restype = c_bool
    dhc_functions.dhc_ctx_init.argtypes = [POINTER(dhc_ctx_t),c_int32,POINTER(c_uint8)]

    if not map:
        c_map = POINTER(c_uint8)()
    else:
        c_map = (c_uint8 * len(map))(*map)

    ctx._map_buf = c_map # dhc.h requires map to stay valid for ctx's whole lifetime;
                         # keep a reference here so ctypes doesn't free it under us

    return dhc_functions.dhc_ctx_init(byref(ctx),dir,c_map)

def block_compress(ctx,output,input,input_size,last_block):
    # bool dhc_block_compress(dhc_ctx_t *ctx, uint8_t *data, uint32_t *data_size_bits, int16_t *samples, uint32_t *sample_size, bool last_block);
    dhc_functions.dhc_block_compress.restype = c_bool
    dhc_functions.dhc_block_compress.argtypes = [POINTER(dhc_ctx_t),POINTER(c_uint8),POINTER(c_uint32),POINTER(c_int16),POINTER(c_uint32),c_bool]

    c_output = (c_uint8 * len(output))(*output)
    c_output_size_bits = c_uint32(len(output)*8)
    c_input = (c_int16 * len(input))(*input)
    c_input_size = c_uint32(input_size)

    dhc_functions.dhc_block_compress(byref(ctx),c_output,byref(c_output_size_bits),c_input,byref(c_input_size),last_block)

    output[:] = list(c_output)
    output_size_bits = int(c_output_size_bits.value)
    consumed_size = int(c_input_size.value)

    return (output_size_bits,consumed_size)

def block_decompress(ctx,output,input,input_size_bits,last_block):
    # bool dhc_block_decompress(dhc_ctx_t *ctx, int16_t *samples, uint32_t *sample_size, uint8_t *data, uint32_t *data_size_bits, bool last_block);
    dhc_functions.dhc_block_decompress.restype = c_bool
    dhc_functions.dhc_block_decompress.argtypes = [POINTER(dhc_ctx_t),POINTER(c_int16),POINTER(c_uint32),POINTER(c_uint8),POINTER(c_uint32),c_bool]

    c_output = (c_int16 * len(output))(*output)
    c_output_size = c_uint32(len(output))
    c_input = (c_uint8 * len(input))(*input)
    c_input_size_bits = c_uint32(input_size_bits)

    dhc_functions.dhc_block_decompress(byref(ctx),c_output,byref(c_output_size),c_input,byref(c_input_size_bits),last_block)

    output[:] = list(c_output)
    output_size = int(c_output_size.value)
    consumed_bits = int(c_input_size_bits.value)

    return (output_size,consumed_bits)

def compress_evaluate(data,size,map=[]):
    # float dhc_compress_evaluate(int16_t *samples, uint32_t sample_size, uint32_t *data_size_bits, uint8_t *map);
    dhc_functions.dhc_compress_evaluate.restype = c_float
    dhc_functions.dhc_compress_evaluate.argtypes = [POINTER(c_int16),c_uint32,POINTER(c_uint32),POINTER(c_uint8)]

    c_data = (c_int16 * len(data))(*data)
    c_size = c_uint32(size)
    c_output_size_bits = (c_uint32 * 1)(0)
    
    if not map:
        c_map = POINTER(c_uint8)()
    else:
        c_map = (c_uint8 * len(map))(*map)

    ratio = dhc_functions.dhc_compress_evaluate(c_data,c_size,c_output_size_bits,c_map)
    output_size_bits = int(c_output_size_bits[0])

    if map:
        map[:] = list(c_map)

    return (ratio,output_size_bits)


def compress(output,input,input_size,map=[]):
    # bool dhc_compress(uint8_t *data, uint32_t *data_size_bits, int16_t *samples, uint32_t sample_size, uint8_t *map);
    dhc_functions.dhc_compress.restype = c_bool
    dhc_functions.dhc_compress.argtypes = [POINTER(c_uint8),POINTER(c_uint32),POINTER(c_int16),c_uint32,POINTER(c_uint8)]

    c_output = (c_uint8 * len(output))(*output)
    c_output_size_bits = (c_uint32 * 1)(0)
    c_input = (c_int16 * len(input))(*input)
    c_input_size = c_uint32(input_size)

    if not map:
        c_map = POINTER(c_uint8)()
    else:
        c_map = (c_uint8 * len(map))(*map)

    status = dhc_functions.dhc_compress(c_output,c_output_size_bits,c_input,c_input_size,c_map)
    
    output[:] = list(c_output)
    output_size_bits = int(c_output_size_bits[0])

    return output_size_bits

def decompress(output,input,input_size_bits,map=[]):
    # bool dhc_decompress(int16_t *samples, uint32_t *sample_size, uint8_t *data, uint32_t data_size_bits, uint8_t *map);

    dhc_functions.dhc_decompress.restype = c_bool
    dhc_functions.dhc_decompress.argtypes = [POINTER(c_int16),POINTER(c_uint32),POINTER(c_uint8),c_uint32,POINTER(c_uint8)]

    c_output = (c_int16 * len(output))(*output)
    c_output_size = (c_uint32 * 1)(0)
    c_input = (c_uint8 * len(input))(*input)
    c_input_size_bits = c_uint32(input_size_bits)

    if not map:
        c_map = POINTER(c_uint8)()
    else:
        c_map = (c_uint8 * len(map))(*map)

    status = dhc_functions.dhc_decompress(c_output,c_output_size,c_input,c_input_size_bits,c_map)
    
    output[:] = list(c_output)
    output_size = int(c_output_size[0])

    return output_size

def main():
    size = randint(100,200)
    # data must have the same limits of int16_t !
    input = [ randint(-100,100) for v in range(size) ]
    input_size = len(input)

    print(f'Array of {input_size} bytes: {input}')
    (ratio,compressed_size_bits) = compress_evaluate(input,input_size)
    print(f'Compress ratio: {ratio:02.2f}, size in bits: {compressed_size_bits}')

    if ratio <= 0:
        print('Not compressing')
    else:
        # compressed vector needs to be twice the input size vector (if ratio > 0)
        # for calling c functions (pre allocated vector)
        compressed = [0]*input_size*2
        compressed_size_bits = compress(compressed,input,input_size)
        print(f'Bit array of {compressed_size_bits} bits: {compressed}')

        output = [0]*input_size
        output_size = decompress(output,compressed,compressed_size_bits)
        print(f'Array of {output_size} bytes: {output}')

        if output == input:
            print('Operation OK!')
        else:
            print('Error!')

if __name__ == '__main__':
    main()

