TARGET = jobasync

KDIR = /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

obj-m := $(TARGET).o

jobasync-objs := sys_asyncjob.o hash.o file_ops.o enc_dec.o read_user_args.o utils.o compression.o

all: xhw3 jobasync

xhw3: xhw3.c
	gcc -pthread -Wall -Werror -I$(INC)/generated/uapi -I$(INC)/uapi xhw3.c -o xhw3 -lcrypto

jobasync:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -rf xhw3
