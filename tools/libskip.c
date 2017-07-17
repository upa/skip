#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <skip_af.h>


static int (*original_socket)(int domain, int type, int protocol);

int socket(int domain, int type, int protocol)
{
	int new_domain = domain;
	
	original_socket = dlsym(RTLD_NEXT, "socket");

	if (domain == AF_INET || domain == AF_INET6) {
#ifdef VERBOSE
		fprintf(stderr,
			"\x1b[1m\x1b[33m"
			"libskip.so: overwrite family %d with AF_SKIP(%d)\n",
			domain, AF_SKIP);
		fprintf(stderr, "\x1b[0m");
#endif
		new_domain = AF_SKIP;
	}

	return original_socket(new_domain, type, protocol);
}


