#!/bin/bash

# Run with 'sudo', needs root access.
# SSD needs to be unmounted.

# Will run for 30mins, 15mins per benchmark.

fio --name=write --ioengine=libaio --iodepth=64 --rw=write --bs=1024k --direct=1 --size=100% --numjobs=12 --runtime=900 --filename=/dev/nvme0n1 --group_reporting=1

fio --name=read --ioengine=libaio --iodepth=64 --rw=read --bs=1024k --direct=1 --size=100% --numjobs=12 --runtime=900 --filename=/dev/nvme0n1 --group_reporting=1
