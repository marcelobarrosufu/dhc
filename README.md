# Delta/Huffman Compressor/Decompressor (DHC)

DHC is an implementation of Delta/Huffman Compressor/Decompressor (DHC) for embedded systems.

How DHC works is described in the article **"An Efficient Lossless Compression Algorithm for Tiny Nodes of Monitoring Wireless Sensor Networks"**, by Francesco Marcelloni and Massimo Vecchio (2009, The Computer Journal, https://doi.org/10.1093/comjnl/bxp035). 

As we could not found any C implementation suitable for embedded systems, we decide to create our own version of the compressor. We developed a decompressor as well, a python binding and all code is now available as open source. 

Authors:

- [Marcelo Barros de Almeida](mailto:marcelobarrosalmeida@gmail.com)
- [Marden Fagundes](mailto:maf_cadastro-github@yahoo.com)

# Using DHC

## DHC API
The DHC API is described in the header file **dhc.h**. 

The compression process consists of using the value differences of successive samples (delta), applying a Huffman dictionary to perform the data reduction (more frequent values are represented by smaller symbols).

The use of DHC can be summarized in 2 steps:

* Check if the dataset is worth compressing by calling the **dhc_compress_evaluate()** function. Positive values in the return of this function indicate that the compression will generate a final buffer smaller than the original one (you should compress the data in this case)
* If it's worth it, compress the data with the **dhc_compress()** function.

Data decompression can be done with the **dhc_decompress()** function.

## Block API

**dhc_compress()**/**dhc_decompress()** need the whole sample/compressed vector available in memory at once. When samples arrive incrementally (e.g. streamed from a sensor in small chunks), use the block API instead, which keeps state across calls in a **dhc_ctx_t** context:

```c
dhc_ctx_t ctx;
dhc_ctx_init(&ctx, DHC_CTX_COMPRESS, map);   // or DHC_CTX_DECOMPRESS

// for each block, until the stream is over:
dhc_block_compress(&ctx, data, &data_size_bits, samples, &sample_size, last_block);
```

`data_size_bits`/`sample_size` work as capacity on input and actual size on output: pass the size of the buffer you are offering, and the function overwrites it with what it actually wrote/consumed. If a buffer fills up before all the input is processed, the call still succeeds and simply consumes less than offered — check the updated size to know how much to resend on the next call. `dhc_compress()`/`dhc_decompress()` are themselves implemented as a single call to the block API with `last_block = true` and no capacity limit.

## Python bindings

The **dhc.py** file inside directory **python_bindings** presents bindings for the python language using ctypes library.
Prior using theses binding you need to compile the the library **dhc_lib.so** for your platform:

```
$ make clean
rm -f *.o *~ tests/*.o app audio_demo audio_block_demo test_block dhc_lib.so
$ make lib
gcc -O0 -g -Wall -std=gnu11  -I. -shared -fPIC -o dhc_lib.so dhc.c
cp dhc_lib.so ./python_bindings/
```

Copy **dhc_lib.so** and **dhc.py** into your python application and use **import dhc** for accessing DHC functions. See **dhc.main()** function for usage example. **python_bindings/test_block.py** has equivalent coverage for the block API (`dhc_ctx_t`/`dhc_block_compress()`/`dhc_block_decompress()`), run it the same way (`python3 test_block.py`) after `make lib`.

## Tests and examples

All demo and test programs live under **tests/**; the resulting binaries (and `dhc_lib.so`) are still produced at the project root by `make`.

A complete example can be seen in the **tests/audio_demo.c** file. In this example, real samples taken from a microphone (**tests/audio.h**) are compressed using DHC. In this example it is possible to turn on or off the custom mapping through the define APP_USE_CUSTOM_MAPPING. **tests/audio_block_demo.c** is the same example rewritten against the block API, feeding/retrieving samples in fixed-size chunks (APP_BLOCK_SIZE) instead of one shot, to show it produces the exact same result.

```
$ make
gcc -O0 -g -Wall -std=gnu11 -I. -c tests/app.c  -o tests/app.o
gcc -O0 -g -Wall -std=gnu11 -I. -c dhc.c  -o dhc.o
gcc -O0 -g -Wall -std=gnu11 -I. -o app tests/app.o dhc.o -lm
gcc -O0 -g -Wall -std=gnu11 -I. -c tests/audio_demo.c  -o tests/audio_demo.o
gcc -O0 -g -Wall -std=gnu11 -I. -o audio_demo tests/audio_demo.o dhc.o -lm
gcc -O0 -g -Wall -std=gnu11 -I. -c tests/audio_block_demo.c  -o tests/audio_block_demo.o
gcc -O0 -g -Wall -std=gnu11 -I. -o audio_block_demo tests/audio_block_demo.o dhc.o -lm
Done

$ ./audio_demo
Compressor rate (21.15%)
Result: OK

$ ./audio_block_demo
Compressor rate (21.15%)
Result: OK
```

**tests/test_block.c** exercises the block API directly: compressing/decompressing split into blocks of varying and irregular sizes (including sizes that force a Huffman code to straddle a block boundary), with and without a custom mapping, forcing partial consumption with undersized buffers, and `dhc_file_ctx_init()`/`dhc_file_block_write()` against `dhc_file_write()`. Run it with `make test`:

```
$ make test
...
./test_block
[blocks of 1 sample, decode chunks of 1 byte] OK (bits=20906)
[irregular blocks, no map, decode chunks of 3 bytes] OK (bits=20906)
[irregular blocks, custom map, decode chunks of 5 bytes] OK (bits=17590)
[single block, no map, decode in one chunk] OK (bits=20906)
[partial_compress] OK (consumed all 50 samples across small 8 byte buffers)
[partial_decompress] OK (decoded all 50 samples 3 at a time)
[file_block_write] OK (matches dhc_file_write byte for byte)

ALL TESTS PASSED
```

Furthermore, as the effectiveness of the DHC depends on the differences between the values of the samples, a file called **tests/app.c** was also created to evaluate the behavior of DHC given a possible range for input values and a data vector size. Random values are generated within the specified range and a compression ratio is then presented as a result of the simulation.

For example, suppose your samples typically range from -50 to 150 and your data vector has 512 samples. It is possible to evaluate the DHC behavior for this case through the following command line (we will repeat this test 10 times):

```
./app -m -50 -M 150 -f 512 -r 10 -v
```

The result can be seen below, with a reduction percentage of 35,18%, on average.

```
Starting 10 repetitions (frame size: 512, interval: [-50,150])
Running test 1/10 reduction=34.84% skipped=0 mapped=0
Running test 2/10 reduction=34.88% skipped=0 mapped=0
Running test 3/10 reduction=34.82% skipped=0 mapped=0
Running test 4/10 reduction=34.72% skipped=0 mapped=0
Running test 5/10 reduction=34.70% skipped=0 mapped=0
Running test 6/10 reduction=34.93% skipped=0 mapped=0
Running test 7/10 reduction=35.03% skipped=0 mapped=0
Running test 8/10 reduction=34.98% skipped=0 mapped=0
Running test 9/10 reduction=35.04% skipped=0 mapped=0
Running test 10/10 reduction=35.18% skipped=0 mapped=0
Fields: Delta,Compress_reduction,skipped,skipped_per,mapped,mapped_perc
200,35.18%,0,0.00%,0,0.00%
```

The dictionary used in the encoding is described in the Table I of the cited article. However, the DHC evaluation function can also generate a better dictionary order suggestion, through a sample frequency evaluation. 

In this case, a mapping array must be passed to **dhc_compress_evaluate()** in order to return the new dictionary. Better results are expected in such situation. However, this also creates the need to transmit the dictionary used, in addition to the compressed data, something that is not covered by  DHC (the mapping and compressed data buffer are generated by DHC but how to transmit both to destination is a user task).

The previous example, we can enable custom dictionary evaluation (**-a** switch), resulting in 45,46% of reduction (when a custom dictionary is used the mapped counter is increased in the output).

```
 ./app -m -50 -M 150 -f 512 -r 10 -v -a
Starting 10 repetitions (frame size: 512, interval: [-50,150])
Running test 1/10 reduction=45.30% skipped=0 mapped=1
Running test 2/10 reduction=45.72% skipped=0 mapped=2
Running test 3/10 reduction=45.88% skipped=0 mapped=3
Running test 4/10 reduction=45.81% skipped=0 mapped=4
Running test 5/10 reduction=45.67% skipped=0 mapped=5
Running test 6/10 reduction=45.58% skipped=0 mapped=6
Running test 7/10 reduction=45.50% skipped=0 mapped=7
Running test 8/10 reduction=45.52% skipped=0 mapped=8
Running test 9/10 reduction=45.45% skipped=0 mapped=9
Running test 10/10 reduction=45.46% skipped=0 mapped=10
Fields: Delta,Compress_reduction,skipped,skipped_per,mapped,mapped_perc
200,45.46%,0,0.00%,10,100.00%
```

# Compiling

Just type **make** to generate the audio demos (audio_demo, audio_block_demo) and the evaluation application (app).
Type **make test** to build and run the block API test suite (test_block).
For python library, type **make lib** to generate dhc_lib.so. 

# Limitations

Sensor data must be represented as **int16_t** (signed 16 bits integers).


