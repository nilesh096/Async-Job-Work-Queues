#!/bin/sh
# test functionality with invalid task id
set -x
touch in.test
./xhw3 -t 12 -e "helloworld" -i in.test -o out.test -w test.out
retval=$?
rm -rf in.test
if test $retval != 0 ; then
        echo xhw3 succeded: $retval, Invalid task number
else
        echo xhw3 program validation failed
        exit 0
fi