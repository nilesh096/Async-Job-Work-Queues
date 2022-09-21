#!/bin/sh
# Test functionality for deletion of file [Success case]
# 2 files input1.txt, input2.txt are created and after successful execution of the syscall, both of them should be deleted

rm -rf input1.txt
rm -rf input2.txt


echo "Hello CSE-506" > "$PWD"/input1.txt
echo "Hello CSE-506" > "$PWD"/input2.txt

./xhw3 -t 1 -i input1.txt,input2.txt -w test_delete.txt

FILE="$PWD"/input1.txt

if [ -f "$FILE" ]; then
    echo "$FILE exists."
    echo Deletion failed
    exit -1
else 
    echo "$FILE does not exist."
fi

FILE="$PWD"/input2.txt

if [ -f "$FILE" ]; then
    echo "$FILE exists."
    echo Deletion failed
    exit -1
else 
    echo "$FILE does not exist."
fi

echo "Files input1.txt and input2.txt deleted successfully"
