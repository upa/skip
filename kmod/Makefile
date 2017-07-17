KERNELSRCDIR := /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(shell pwd)
VERBOSE = 0

obj-m := skip.o
skip-objs := skip_lwt.o skip_common.o

ccflags-y := -I../include

all:
	make -C $(KERNELSRCDIR) M=$(BUILD_DIR) V=$(VERBOSE) modules

clean:
	make -C $(KERNELSRCDIR) M=$(BUILD_DIR) clean
