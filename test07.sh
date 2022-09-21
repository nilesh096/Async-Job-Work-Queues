#!/bin/sh
# Test functionality for concatenation of file [Success case]
# 3 files input1.txt, input2.txt and input3.txt are created and after successful execution of the syscall
# contents of all the 3 files should be written to the output.txt

rm -rf "$PWD"/input1.txt
rm -rf "$PWD"/input2.txt
rm -rf "$PWD"/input3.txt
rm -rf "$PWD"/output.txt
rm -rf "$PWD"/test.txt
rm -rf "$PWD"/test_concat.txt


echo "Hello CSE-506_1" > "$PWD"/input1.txt
echo "Hello CSE-506_2" > "$PWD"/input2.txt
echo "Hello CSE-506_3" > "$PWD"/input3.txt

./xhw3 -t 3 -i input1.txt,input2.txt,input3.txt -o output.txt -w test_concat.txt

cat "$PWD"/input1.txt "$PWD"/input2.txt "$PWD"/input3.txt >> "$PWD"/test.txt

if cmp output.txt test.txt ; then
    echo "sys_asyncjob: input and output files contents are the same"
    exit 0
else
    echo "sys_asyncjob: input and output files contents DIFFER"
    exit 1
fi

echo "Files input1.txt and input2.txt concatenated successfully"
