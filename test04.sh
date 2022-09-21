#!/bin/sh
# Test functionality for listing of jobs [Success case]
# Here we are calling no_op task which will sleep for 20 seconds

rm -rf "$PWD"/no_op.txt
rm -rf "$PWD"/job_status_list.txt

echo "Hello CSE-506" > "$PWD"/input1.txt

jobid=$(./xhw3 -t 10 -w no_op.txt | sed -n '2p' | cut -d " " -f3)
./xhw3 -j 4 -w job_status_list.txt

if [ -s job_status_list.txt ]; then
    echo "Job list successfully completed"
    exit 0
else
    echo "job listing failed"
    exit -1
fi
