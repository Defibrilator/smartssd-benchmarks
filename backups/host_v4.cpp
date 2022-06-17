/**
* Copyright (C) 2019-2021 Xilinx, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License"). You may
* not use this file except in compliance with the License. A copy of the
* License is located at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations
* under the License.
*/

#include "cmdlineparser.h"
#include <iostream>
#include <cstring>
#include <chrono>

#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iosfwd>
#include <unistd.h>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define DATA_SIZE (2000000000/4)

double throughput_from_fpga_max_host_to_ssd = 0;
double throughput_from_fpga_max_ssd_to_host = 0;
double throughput_from_cpu_max_host_to_ssd = 0;
double throughput_from_cpu_max_ssd_to_host = 0;

////////////////////////////////////////////////////////////////////////////////
class Timer {
    std::chrono::high_resolution_clock::time_point mTimeStart;

   public:
    Timer() { reset(); }
    long long stop() {
        std::chrono::high_resolution_clock::time_point timeEnd = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - mTimeStart).count();
    }
    void reset() { mTimeStart = std::chrono::high_resolution_clock::now(); }
};

Timer global_timer;

std::pair<double, double> p2p_host_to_ssd(int& nvmeFd, xrtDeviceHandle device, xrt::kernel& krnl) {
	Timer timer_from_cpu, timer_from_fpga;
    int ret = 0;
    //int size = DATA_SIZE;
    size_t vector_size_bytes = sizeof(int) * DATA_SIZE;

    xrt::bo::flags flags = xrt::bo::flags::p2p;
    //auto bo0 = xrt::bo(device, vector_size_bytes, krnl.group_id(0));
    auto p2p_bo1 = xrt::bo(device, vector_size_bytes, flags, krnl.group_id(1));

    std::cout << "Map the contents of the buffer object into host memory : " << global_timer.stop() << std::endl;
    //auto bo0_map = bo0.map<int*>();
    auto p2p_bo1_map = p2p_bo1.map<int*>();

    std::cout << "Start cpu timer : " << global_timer.stop() << std::endl;
    timer_from_cpu = Timer();
    
    /*std::cout << "Fill the map : " << global_timer.stop() << std::endl;
    std::fill(bo0_map, bo0_map + size, 1);
    std::fill(p2p_bo1_map, p2p_bo1_map + size, 1);*/

    std::cout << "Synchronize input buffer data to device global memory : " << global_timer.stop() << std::endl;
    p2p_bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    /*std::cout << "Execution of the kernel : " << global_timer.stop() << std::endl;
    auto run = krnl(bo0, p2p_bo1, size);
    run.wait();*/

    std::cout << "Start fpga timer : " << global_timer.stop() << std::endl;
    timer_from_fpga = Timer();

    std::cout << "Now start P2P Write from device buffers to SSD : " << global_timer.stop() << std::endl;
    ret = pwrite(nvmeFd, (void*)p2p_bo1_map, vector_size_bytes, 0);
    if (ret == -1) std::cout << "P2P: write() failed, err: " << ret << ", line: " << __LINE__ << std::endl;

    std::cout << "Stop timers : " << global_timer.stop() << std::endl;
    long long duration_from_cpu = timer_from_cpu.stop();
    long long duration_from_fpga = timer_from_fpga.stop();

    std::cout << "Compute throughputs : " << global_timer.stop() << std::endl;
    double throughput = vector_size_bytes;
    throughput *= 1000000;     // convert us to s;
    throughput /= 1024 * 1024; // convert to MB

    double throughput_from_fpga = throughput / duration_from_fpga;
    double throughput_from_cpu = throughput / duration_from_cpu;

    if (throughput_from_fpga > throughput_from_fpga_max_host_to_ssd) {
    	throughput_from_fpga_max_host_to_ssd = throughput_from_fpga;
    }

    if (throughput_from_cpu > throughput_from_cpu_max_host_to_ssd) {
    	throughput_from_cpu_max_host_to_ssd = throughput_from_cpu;
    }

    return std::make_pair(throughput_from_fpga, throughput_from_cpu);
}

std::pair<double, double> p2p_ssd_to_host(int& nvmeFd, xrtDeviceHandle device, xrt::kernel& krnl) {
	Timer timer_from_cpu, timer_from_fpga;
    //int size = DATA_SIZE;
    size_t vector_size_bytes = sizeof(int) * DATA_SIZE;

    xrt::bo::flags flags = xrt::bo::flags::p2p;
    std::cout << "Allocate Buffer in Global Memory : " << global_timer.stop() << std::endl;
    auto p2p_bo0 = xrt::bo(device, vector_size_bytes, flags, krnl.group_id(0));
    //auto bo_out = xrt::bo(device, vector_size_bytes, krnl.group_id(1));

    std::cout << "Map the contents of the buffer object into host memory : " << global_timer.stop() << std::endl;
    auto p2p_bo0_map = p2p_bo0.map<int*>();
    //auto bo_out_map = bo_out.map<int*>();

    //std::cout << "Fill the map : " << global_timer.stop() << std::endl;
    //std::fill(p2p_bo0_map, p2p_bo0_map + size, 0);
    //std::fill(bo_out_map, bo_out_map + size, 0);

    std::cout << "Start timers : " << global_timer.stop() << std::endl;
    timer_from_cpu = Timer();
    timer_from_fpga = Timer();

    std::cout << "Now start P2P Read from SSD to device buffers : " << global_timer.stop() << std::endl;
    if (pread(nvmeFd, (void*)p2p_bo0_map, vector_size_bytes, 0) <= 0) {
        std::cerr << "ERR: pread failed: "
                  << " error: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Stop timer : " << global_timer.stop() << std::endl;
    long long duration_from_fpga = timer_from_fpga.stop();

    double throughput = vector_size_bytes;
    throughput *= 1000000;     // convert us to s;
    throughput /= 1024 * 1024; // convert to MB

    double throughput_from_fpga = throughput / duration_from_fpga;

    if (throughput_from_fpga > throughput_from_fpga_max_ssd_to_host) {
    	throughput_from_fpga_max_ssd_to_host = throughput_from_fpga;
    }

    /*std::cout << "Execution of the kernel : " << global_timer.stop() << std::endl;
    auto run1 = krnl(p2p_bo0, bo_out, size);
    run1.wait();*/

    // Get the output data from the device
    p2p_bo0.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    long long duration_from_cpu = timer_from_cpu.stop();
    double throughput_from_cpu = throughput / duration_from_cpu;

    if (throughput_from_cpu > throughput_from_cpu_max_ssd_to_host) {
    	throughput_from_cpu_max_ssd_to_host = throughput_from_cpu;
    }

    return std::make_pair(throughput_from_fpga, throughput_from_cpu);
}

int main(int argc, char** argv) {
    // Command Line Parser
    sda::utils::CmdLineParser parser;

    global_timer = Timer();

    // Switches
    //**************//"<Full Arg>",  "<Short Arg>", "<Description>", "<Default>"
    parser.addSwitch("--xclbin_file", "-x", "input binary file string", "");
    parser.addSwitch("--device_id", "-d", "device index", "0");
    parser.addSwitch("--iterations", "-i", "number of iterations", "1000000");
    parser.addSwitch("--file_path", "-p", "file path string", "");
    parser.parse(argc, argv);

    // Read settings
    std::string binaryFile = parser.value("xclbin_file");
    int device_index = stoi(parser.value("device_id"));
    int num_iter = stoi(parser.value("iterations"));
    std::string filepath = parser.value("file_path");

    if (argc < 5) {
        parser.printHelp();
        return EXIT_FAILURE;
    }

    Timer timer = Timer();

    std::cout << "Open the device" << device_index << std::endl;
    auto device = xrt::device(device_index);
    std::cout << "Load the xclbin " << binaryFile << std::endl;
    auto uuid = device.load_xclbin(binaryFile);

    auto krnl = xrt::kernel(device, uuid, "dummy_kernel");

    std::cout << "\nStarting " << num_iter << " iterations W/R\n";
    int nvmeFd = -1;
    double sum_write_throughput_from_fpga = 0;
    double sum_read_throughput_from_fpga = 0;
    double sum_write_throughput_from_cpu = 0;
    double sum_read_throughput_from_cpu = 0;
    for (int i=0; i<num_iter; i++) {
    	std::cout << "Iteration " << i << " : " << global_timer.stop() << std::endl;
        std::cout << "P2P transfer from host to SSD" << " : " << global_timer.stop() << std::endl;
        // Get access to the NVMe SSD.
        nvmeFd = open(filepath.c_str(), O_RDWR | O_DIRECT);
        if (nvmeFd < 0) {
            std::cerr << "ERROR: open " << filepath << "failed: " << std::endl;
            return EXIT_FAILURE;
        }
        auto p1 = p2p_host_to_ssd(nvmeFd, device, krnl);
        sum_write_throughput_from_fpga += p1.first;
        sum_write_throughput_from_cpu += p1.second;
        (void)close(nvmeFd);

        std::cout << "P2P transfer from SSD to host" << " : " << global_timer.stop() << std::endl;
        nvmeFd = open(filepath.c_str(), O_RDWR | O_DIRECT);
        if (nvmeFd < 0) {
            std::cerr << "ERROR: open " << filepath << "failed: " << std::endl;
            return EXIT_FAILURE;
        }
        auto p2 = p2p_ssd_to_host(nvmeFd, device, krnl);
        sum_read_throughput_from_fpga += p2.first;
        sum_read_throughput_from_cpu += p2.second;
        (void)close(nvmeFd);
    }

    double average_write_throughput_from_fpga = sum_write_throughput_from_fpga / num_iter;
    double average_read_throughput_from_fpga = sum_read_throughput_from_fpga / num_iter;
    double average_write_throughput_from_cpu = sum_write_throughput_from_cpu / num_iter;
    double average_read_throughput_from_cpu = sum_read_throughput_from_cpu / num_iter;

    std::cout << "\nWrite bandwidth achieved :\n"
    		  << "		Max throughput from cpu: " << throughput_from_cpu_max_host_to_ssd << " MB/s\n"
              << "		Average throughput from cpu: " << average_write_throughput_from_cpu << " MB/s\n\n"
    		  << "		Max throughput from fpga: " << throughput_from_fpga_max_host_to_ssd << " MB/s\n"
              << "		Average throughput from fpga: " << average_write_throughput_from_fpga << " MB/s\n";

    std::cout << "\nRead bandwidth achieved :\n"
    		  << "		Max throughput from cpu: " << throughput_from_cpu_max_ssd_to_host << " MB/s\n"
              << "		Average throughput from cpu: " << average_read_throughput_from_cpu << " MB/s\n\n"
    		  << "		Max throughput from fpga: " << throughput_from_fpga_max_ssd_to_host << " MB/s\n"
              << "		Average throughput from fpga: " << average_read_throughput_from_fpga << " MB/s\n";

    long long duration = timer.stop() / 1000000;// convert us to s;   
    long long min = duration / 60;
    long long sec = duration % 60; 

    std::cout << "\nTotal time: " << min << "m" << sec << "s\n";

    std::cout << "\nFINISHED\n";
    return 0;
}