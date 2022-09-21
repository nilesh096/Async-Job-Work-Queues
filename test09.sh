#!/bin/sh
# Test functionality for deleting a job [Success case]
# Here we are calling no_op task which will sleep for 20 seconds

rm -rf "$PWD"/no_op.txt
rm -rf "$PWD"/job_status_delete.txt

jobid=$(./xhw3 -t 10 -w no_op.txt | sed -n '2p' | cut -d " " -f3)
./xhw3 -j 1 -n $jobid -w job_status_delete.txt

if [ -s job_status_delete.txt ]; then
    echo "Job deletion successfully completed"
    exit 0
else
    echo "job deletion failed"
    exit -1
fi
