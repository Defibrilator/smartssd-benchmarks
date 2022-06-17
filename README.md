# Benchmarks

We are working with the U.2 SmartSSD from Xilinx. These benhmarks are made to test the throughput capabilities of the SmartSSD

## Without FPGA

The first benchmark is done with FIO, it tests the strict capabilities of the SSD without use of the FPGA. The results and the commands used can be be found inside **nofgpa_fio_benchmarks.txt**. 

Four benchmarks were done, random read/write, and sequential read/write.

## With FPGA

Here we are conducting a throughput benchmark using the FPGA as DMA.

The code can be found inside **bin/benchmark.cpp** and can be run with 

> `./bin/benchmark -x ./bin/empty_kernel.xclbin -p <file path on the smartssd> -i <number of iterations>`

An empty kernel is used because the data doesn't need to be modified on the fpga logic, but a kernel is still needed because the buffers are defined with it.

The SSD has an ext4 filesystem with an empty file on it.

### From Host Memory

Unfortunatly, the U.2 platform on the SmartSSD does not support host memory access from the kernel. All data needs to be copied to the FPGA's global memory before use. More information can be found here https://xilinx.github.io/XRT/master/html/hm.html and here https://xilinx.github.io/Vitis-Tutorials/2021-2/build/html/docs/Hardware_Acceleration/Feature_Tutorials/08-using-hostmem/README.html.

### Code 

We first start in main by opening the device and loading the binary of the kernel inside. We also define a 2GB buffer for writing. We define it with the p2p flags, p2p flags are used for data transfer between an FPGA and an NVMe device. We define a `buffer_map` to map the contents of the buffer object into host memory. We use the map to fill the buffer with the value 1. This is a long operation (~30s) since there is 2gb to fill. We do it in the main so that it is done only once. It cannot be skipped as `pwrite()` will not work without it.

Then, we start the iterations, first is the WRITE operation with the call of `p2p_host_to_ssd()` with the buffer and buffer_map as arguments. Before calling the function, we open the file on the SSD.

The member function `sync` on the buffer synchronizes the buffer data to the FPGA's global memory. 

Then we use `pwrite()` from `unistd.h` to write the data inside the buffer on the SSD file from the buffer map. 

We use the user defined `Timer` class to compute the throughput and return it to `main`. Two timers are used, one starts before the function `sync` to compute the throughput from CPU to SSD, the second after `sync` to compute the throughput from FPGA to SSD.

The second part of the iteration is the call to `p2p_ssd_to_host()` which is very similar to `p2p_host_to_ssd()` but defines its own buffers. `pread()` is used on the buffer map instead of `pwrite()`.

### Results afer 300 iterations

Write bandwidth achieved :
- Max throughput from cpu: 1603.93 MB/s
- Average throughput from cpu: 1524.55 MB/s
- Max throughput from fpga: 1603.99 MB/s
- Average throughput from fpga: 1524.61 MB/s

Read bandwidth achieved :
- Max throughput from cpu: 2924.12 MB/s
- Average throughput from cpu: 2231.27 MB/s
- Max throughput from fpga: 2924.26 MB/s
- Average throughput from fpga: 2231.35 MB/s

Total time: 14m45s
