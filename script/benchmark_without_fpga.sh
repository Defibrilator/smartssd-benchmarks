#!/bin/bash

# Run with 'sudo', needs root access.

# Will run for 2h, 30mins per benchmark.

fio --name=rand-write --ioengine=libaio --iodepth=256 --rw=randwrite --bs=4k --direct=1 --size=100% --numjobs=12 --runtime=1800 --filename=/dev/nvme0n1 --group_reporting=1

fio --name=rand-read --ioengine=libaio --iodepth=256 --rw=randread --bs=4k --direct=1 --size=100% --numjobs=12 --runtime=1800 --filename=/dev/nvme0n1 --group_reporting=1

fio --name=seq-write --ioengine=libaio --iodepth=64 --rw=write --bs=1024k --direct=1 --size=100% --numjobs=12 --runtime=1800 --filename=/dev/nvme0n1 --group_reporting=1

fio --name=seq-read --ioengine=libaio --iodepth=64 --rw=read --bs=1024k --direct=1 --size=100% --numjobs=12 --runtime=1800 --filename=/dev/nvme0n1 --group_reporting=1
