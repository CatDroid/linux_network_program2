target = test
obj-m := test.o  
#generate the path  
	CURRENT_PATH:=$(shell pwd)  
#the current kernel version number  
	LINUX_KERNEL:=$(shell uname -r)  
#the absolute path  
LINUX_KERNEL_PATH:=/usr/src/linux-headers-$(LINUX_KERNEL) 
#complie object  
all:  
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH)
#clean  
clean:  
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH)
