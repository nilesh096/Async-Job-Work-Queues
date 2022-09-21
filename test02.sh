#!/bin/sh
# Test functionality for hash of a file [Success case]
# File input1.txt is created and after successful execution of the syscall
# the output of hash should be written to output.txt

rm -rf "$PWD"/input1.txt
rm -rf "$PWD"/output.txt

echo "Hello CSE-506" > "$PWD"/input1.txt

./xhw3 -t 4 -i input1.txt -o output.txt -w test_hash.txt

val=$(sha256sum input1.txt | tr -s ' ' | cut -d ' ' -f 1)
val1=`echo -n $val | perl -pe 's/([0-9a-f]{2})/chr hex $1/gie'`

val2=$(cat output.txt)

if [ "$val1" = "$val2" ]; then
    echo "SHA256 of file input.txt is equal to the output of the sha256sum command"
    exit 0
else
    echo "SHA256 hash of file is invalid"
    exit -1
fi


