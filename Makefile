

iproute2-src = iproute2-4.10.0
env_vars = LIBSKIP_VERBOSE=yes

subdirs = kmod tools $(iproute2-src)

all:
	for i in $(subdirs); do \
		echo; echo $$i; \
		make -C $$i $(env_vars); \
		done

clean:
	for i in $(subdirs); do \
		echo; echo $$i; \
		make -C $$i clean; \
		done
