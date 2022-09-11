# Delta/Huffman Compressor (DHC)

DHC is an implementation of Delta/Huffman Compressor (DHC) for embedded systems.

How DHC works is described in the article **"An Adaptive Lossless Data Compression Scheme for Wireless Sensor Networks"**, by Eugenio Martinelli, Jonathan Gana Kolo, S. Anandan Shanmugam, David Wee Gin Lim, Li-Minn Ang, Kah Phooi Seng (2012, Journal of Sensors, https://doi.org/10.1155/2012/539638). It is available as open access [here](https://www.hindawi.com/journals/js/2012/539638/).

As I could not found any C implementation suitable for embedded systems, I decide to create my own version of the compressor. I developed a decompressor as well, a python binding and all code is now available as open source.

# Using DHC

## DHC API
The DHC API is described in the header file **dhc.h**. 

The compression process consists of using the value differences of successive samples (delta), applying a Huffman dictionary to perform the data reduction (more frequent values are represented by smaller symbols).

The use of DHC can be summarized in 2 steps:

* Check if the dataset is worth compressing by calling the **dhc_compress_evaluate()** function. Positive values in the return of this function indicate that the compression will generate a final buffer smaller than the original one (you should compress the data in this case)
* If it's worth it, compress the data with the **dhc_compress()** function.

Data decompression can be done with the **dhc_decompress()** function.

The **dhc.py** file presents bindings for the python language using ctypes library.

## Examples

A complete example can be seen in the **audio_demo.c** file. In this example, real samples taken from a microphone (**audio.h**) are compressed using DHC.

Furthermore, as the effectiveness of the DHC depends on the differences between the values of the samples, a file called **app.c** was also created to evaluate the behavior of DHC given a possible range for input values and a data vector size. Random values are generated within the specified range and a compression ratio is then presented as a result of the simulation.

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

# Limitations

Sensor data must be represented as **int16_t** (signed 16 bits integers).


