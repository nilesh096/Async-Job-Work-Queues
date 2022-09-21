#!/bin/sh
# Test functionality for renaming of files [Success case]
# 2 files input1.txt, input2.txt are created and after successful execution of the syscall,
# both of them should be renamed to output1.txt and output2.txt respectively

rm -rf "$PWD"/input1.txt
rm -rf "$PWD"/input2.txt
rm -rf "$PWD"/output1.txt
rm -rf "$PWD"/output2.txt
rm -rf "$PWD"/test_rename.txt

val="Hello CSE-506"
echo $val > "$PWD"/input1.txt
echo $val > "$PWD"/input2.txt

./xhw3 -t 7 -i input1.txt,input2.txt -o output1.txt,output2.txt -w test_rename.txt

FILE="$PWD"/input1.txt

if [ -f "$FILE" ]; then
    echo "$FILE exists."
    echo Rename failed
    exit -1
else 
    echo "$FILE does not exist."
fi

FILE="$PWD"/input2.txt

if [ -f "$FILE" ]; then
    echo "$FILE exists."
    echo Rename failed
    exit -1
else 
    echo "$FILE does not exist."
fi

cat1=$(cat "$PWD"/output1.txt)
cat2=$(cat "$PWD"/output2.txt)

ret=0
if [ "$val" = "$cat1" ]; then
    echo "Rename of file input1.txt to output1.txt is successful"
else
    echo "Rename of file input2.txt to output2.txt failed"
    ret=1
fi

if [ "$val" = "$cat1" ]; then
    echo "Rename of file input1.txt to output1.txt is successful"
else
    echo "Rename of file input1.txt to output1.txt failed"
    ret=1
fi

if [ "$ret" = 0 ]; then echo "Rename of files successful"; exit $ret ; fi
echo "Rename of files failed"; exit $ret
