#!/usr/bin/python3
"""
An implementation of Delta/Huffman Compressor (DHC) for embedded systems.

Copyright (c) 2022
Marcelo Barros de Almeida <marcelobarrosalmeida@gmail.com>
Marden Fagundes <maf_cadastro-github@yahoo.com>
"""

from random import randint

import dhc

def block_compress_all(samples,block_sizes,map=[]):
    ctx = dhc.dhc_ctx_t()
    dhc.ctx_init(ctx,dhc.DHC_CTX_COMPRESS,map)

    compressed = bytearray()
    total_bits = 0
    pos = 0

    for i,block_size in enumerate(block_sizes):
        last_block = (i == len(block_sizes)-1)
        chunk = samples[pos:pos+block_size]
        output = [0]*(block_size*4+4) # generous: up to 30 bits/sample fits easily

        (out_bits,consumed) = dhc.block_compress(ctx,output,chunk,len(chunk),last_block)

        assert consumed == len(chunk), f'block_compress only consumed {consumed}/{len(chunk)} samples'

        compressed.extend(output[0:(out_bits+7)//8])
        total_bits += out_bits
        pos += consumed

    return (bytes(compressed),total_bits)

def block_decompress_all(data,data_size_bits,sample_count,chunk_bytes,map=[]):
    ctx = dhc.dhc_ctx_t()
    dhc.ctx_init(ctx,dhc.DHC_CTX_DECOMPRESS,map)

    decoded = []
    bit_pos = 0

    while bit_pos < data_size_bits:
        remaining_bits = data_size_bits - bit_pos
        chunk_bits = min(chunk_bytes*8,remaining_bits)
        last_block = (bit_pos + chunk_bits) >= data_size_bits

        chunk = list(data[bit_pos//8 : bit_pos//8 + (chunk_bits+7)//8])
        output = [0]*(sample_count - len(decoded))

        (got,consumed_bits) = dhc.block_decompress(ctx,output,chunk,chunk_bits,last_block)

        decoded.extend(output[0:got])
        bit_pos += consumed_bits

    return decoded

def test_roundtrip(name,samples,block_sizes,decode_chunk_bytes,map=[]):
    output = [0]*(len(samples)*4+4)
    ref_bits = dhc.compress(output,samples,len(samples),map)
    ref_data = bytes(output[0:(ref_bits+7)//8])

    (blk_data,blk_bits) = block_compress_all(samples,block_sizes,map)

    assert blk_bits == ref_bits, f'[{name}] bit size mismatch (block={blk_bits}, ref={ref_bits})'
    assert blk_data == ref_data, f'[{name}] compressed bytes differ from reference'

    decoded = block_decompress_all(blk_data,blk_bits,len(samples),decode_chunk_bytes,map)

    assert decoded == samples, f'[{name}] decoded samples differ'

    print(f'[{name}] OK (bits={blk_bits})')

def test_partial_compress():
    samples = [randint(-20,20) for _ in range(50)]
    ctx = dhc.dhc_ctx_t()
    dhc.ctx_init(ctx,dhc.DHC_CTX_COMPRESS,[])

    total_consumed = 0

    while total_consumed < 50:
        output = [0]*8 # tiny output capacity
        chunk = samples[total_consumed:]

        (out_bits,consumed) = dhc.block_compress(ctx,output,chunk,len(chunk),False)

        assert consumed > 0, 'partial_compress made no progress with an 8 byte buffer'

        total_consumed += consumed

    dhc.block_compress(ctx,[0]*8,[],0,True)

    print(f'[partial_compress] OK (consumed all {total_consumed} samples across small 8 byte buffers)')

def test_partial_decompress():
    samples = [randint(-20,20) for _ in range(50)]
    output = [0]*400
    compressed_bits = dhc.compress(output,samples,50,[])
    compressed = bytes(output[0:(compressed_bits+7)//8])

    ctx = dhc.dhc_ctx_t()
    dhc.ctx_init(ctx,dhc.DHC_CTX_DECOMPRESS,[])

    decoded = []
    bit_pos = 0

    while len(decoded) < 50:
        remaining_bits = compressed_bits - bit_pos
        chunk = list(compressed[bit_pos//8 : bit_pos//8 + (remaining_bits+7)//8])
        out = [0]*3 # tiny output capacity

        (got,consumed_bits) = dhc.block_decompress(ctx,out,chunk,remaining_bits,True)

        assert got > 0, 'partial_decompress made no progress'

        decoded.extend(out[0:got])
        bit_pos += consumed_bits

    assert decoded == samples, 'partial_decompress: decoded samples differ'

    print('[partial_decompress] OK (decoded all 50 samples 3 at a time)')

def main():
    samples = [randint(-100,100) for _ in range(2000)]

    test_roundtrip('blocks of 1 sample, decode chunks of 1 byte',samples,[1]*2000,1)

    block_sizes = [1,2,3,5,7,11,13,17,19,23,29,37,41,53,97,131,197,311,499,733]
    block_sizes[-1] += len(samples) - sum(block_sizes)
    test_roundtrip('irregular blocks, no map, decode chunks of 3 bytes',samples,block_sizes,3)

    map = [0]*dhc.DHC_TABLE_SIZE
    dhc.compress_evaluate(samples,len(samples),map)
    test_roundtrip('irregular blocks, custom map, decode chunks of 5 bytes',samples,block_sizes,5,map)

    test_roundtrip('single block, no map, decode in one chunk',samples,[len(samples)],len(samples)*4)

    test_partial_compress()
    test_partial_decompress()

    print('\nALL TESTS PASSED')

if __name__ == '__main__':
    main()
