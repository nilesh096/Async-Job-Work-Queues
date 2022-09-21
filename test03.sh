#!/bin/sh
# Test functionality for enc/dec of a file [Success case]
# File input1.txt is created and after successful execution of the syscall
# the encrypted input1.txt should be written to output.txt

rm -rf "$PWD"/input1.txt
rm -rf "$PWD"/output_enc.txt
rm -rf "$PWD"/output_dec.txt

echo "Hello CSE-506" > "$PWD"/input1.txt

./xhw3 -t 5 -i input1.txt -o output_enc.txt -e password1! -w test_hash_enc.txt

sleep 10

./xhw3 -t 6 -i output_enc.txt -o output_dec.txt -e password1! -w test_hash_dec.txt

if cmp input1.txt output_dec.txt ; then
        echo "sys_asyncjob: input and output files contents are the same"
        exit 0
else
        echo "sys_asyncjob: input and output files contents DIFFER"
        exit 1
fi
