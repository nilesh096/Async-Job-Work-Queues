#!/bin/sh
# test functionality with password length < 6
set -x
touch in.test
./xhw3 -t 5 -e "hello" -i in.test -o out.test
retval=$?
rm -rf in.test
if test $retval != 0 ; then
        echo xhw3 succeded: $retval, Password length should be greater than 6
else
        echo xhw3 program failed
        exit 0
fi

