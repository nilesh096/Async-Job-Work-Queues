#!/bin/sh
# Test functionality for compression/decompression of a file [Success case]
# File input1.txt is created and after successful execution of the syscall
# the compressed input1.txt should be written to output.txt

rm -rf "$PWD"/input1.txt
rm -rf "$PWD"/output_com.txt
rm -rf "$PWD"/output_decom.txt

echo "Hello CSE-506" > "$PWD"/input1.txt

./xhw3 -t 8 -i input1.txt -o output_com.txt -w test_com.txt
sleep 10
./xhw3 -t 9 -i output_com.txt -o output_decom.txt -w test_decom.txt

if cmp input1.txt output_decom.txt ; then
    echo "sys_asyncjob: input and output files contents are the same"
    exit 0
else
    echo "sys_asyncjob: input and output files contents DIFFER"
    exit 1
fi
