#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <af_skip.h>


#define PROGNAME	"libskip.so"


#ifdef VERBOSE
#define pr_v(fmt, ...) fprintf(stderr,					\
			       "\x1b[1m\x1b[32m" PROGNAME ": %s: " fmt	\
			       "\x1b[0m",				\
			       __func__, ##__VA_ARGS__)

#define pr_vs(fmt, ...) fprintf(stderr,					\
				"\x1b[1m\x1b[34m" PROGNAME ": %s: " fmt	\
				"\x1b[0m",				\
				__func__, ##__VA_ARGS__)
#else
#define pr_v(fmt, ...)
#define pr_vs(fmt, ...)
#endif

#define pr_e(fmt, ...) fprintf(stderr,					\
			       "\x1b[1m\x1b[31m" PROGNAME ": %s: " fmt	\
			       "\x1b[0m",				\
			       __func__, ##__VA_ARGS__)




static int (*original_socket)(int domain, int type, int protocol);

int socket(int domain, int type, int protocol)
{
	int ret;
	int new_domain = domain;
	
	original_socket = dlsym(RTLD_NEXT, "socket");

	if (domain == AF_INET || domain == AF_INET6) {
		pr_vs("overwrite family %d with AF_SKIP (%d)\n",
		      domain, AF_SKIP);
		new_domain = AF_SKIP;
	} else {
		pr_v("not AF_INET/6 family '%d'. call original socket()\n",
		     domain);
	}

	ret = original_socket(new_domain, type, protocol);
	if (ret < 0)
		pr_e("failed '%d': %s\n", ret, strerror(errno));

	return ret;
}




static int (*original_bind)(int sockfd, const struct sockaddr *addr,
			    socklen_t addrlen);

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	int ret;
	int new_addrlen;
	unsigned short  port;
	char *str_bind_addr, buf[64];
	struct sockaddr_storage saddr_s;
	struct sockaddr_in *sa4 = (struct sockaddr_in*)&saddr_s;
	struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&saddr_s;

	original_bind = dlsym(RTLD_NEXT, "bind");

	switch (addr->sa_family) {
	case AF_INET:
		port = ((struct sockaddr_in *)addr)->sin_port;
		break;
	case AF_INET6:
		port = ((struct sockaddr_in6 *)addr)->sin6_port;
		break;
	default :
		pr_v("not AF_INET/6 family '%d'. call original bind()\n",
		     addr->sa_family);
		return original_bind(sockfd, addr, addrlen);
	}


	str_bind_addr = getenv("AF_SKIP_BIND_ADDRESS");

	if (!str_bind_addr) {
		pr_v("AF_SKIP_BIND_ADDRESS is not defined. "
		     "call original bind()\n");
		return original_bind(sockfd, addr, addrlen);
	}

	if (inet_pton(AF_INET, str_bind_addr, &sa4->sin_addr) == 1)  {

		inet_ntop(AF_INET,
			  &(((struct sockaddr_in *)addr)->sin_addr),
			  buf, sizeof(buf));

		sa4->sin_family = AF_INET;
		sa4->sin_port = port;
		new_addrlen = sizeof(struct sockaddr_in);
		pr_vs("bind() address is changed from %s to %s\n",
		      buf, str_bind_addr);


	} else if (inet_pton(AF_INET6, str_bind_addr, &sa6->sin6_addr) == 1)  {

		inet_ntop(AF_INET6,
			  &(((struct sockaddr_in6 *)addr)->sin6_addr),
			  buf, sizeof(buf));

		sa6->sin6_family = AF_INET6;
		sa6->sin6_port = port;
		new_addrlen = sizeof(struct sockaddr_in6);
		pr_vs("bind() address is changed to from %s to %s\n",
		      buf, str_bind_addr);

	} else {
		pr_e("invalid bind address '%s'\n", str_bind_addr);
		return -EINVAL;
	}

	ret = original_bind(sockfd, (struct sockaddr *)&saddr_s, new_addrlen);
	if (ret)
		pr_e("failed '%d': %s\n", ret, strerror(errno));

	return ret;

}

