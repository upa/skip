KERNELSRCDIR := /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(shell pwd)
VERBOSE = 0

obj-m := skip.o
skip-objs := skip_main.o skip_lwt.o af_skip.o

ccflags-y := -I$(src)/../include/

all:
	echo $(ccflags-y)
	make -C $(KERNELSRCDIR) M=$(BUILD_DIR) V=$(VERBOSE) modules

clean:
	make -C $(KERNELSRCDIR) M=$(BUILD_DIR) clean
