

CC = gcc 
CLAGS := -g -Wall
INCLUDE := -I../include/
LDL_CFLAGS := -fPIC -shared -ldl

PROGNAME = libskip.so


LIBSKIP_VERBOSE ?= yes
flag_verbose_yes = -DVERBOSE
flag_verbose_no = 

all: $(PROGNAME)


libskip.so: libskip.c
	$(CC) libskip.c $(INCLUDE) $(CFLAGS) $(LDL_CFLAGS) \
		$(flag_verbose_$(LIBSKIP_VERBOSE)) -o $@ 

clean:
	rm $(PROGNAME)
