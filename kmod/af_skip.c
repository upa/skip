
/* af_skip.c
 *
 * skip over socket processing
 *
 * Address family implementation of the skip based on a thin socket
 * layer connecting a socket opened at a (container) netns and a
 * socket opened at the host network stack (default netns).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <net/dst.h>
#include <net/route.h>
#include <net/ip6_route.h>

#include <skip_lwt.h>
#include <af_skip.h>

#include "skip.h"


#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt



struct skip_sock {
	struct sock sk;

	bool bound;		/* bind() is called or not */

	struct socket *sock;	/* this socket */

	struct socket *vsock;	/* socket with original family at namespace */
	struct socket *hsock;	/* socket with original family at host */
};

static inline struct skip_sock *skip_sk(const struct sock *sk)
{
	return (struct skip_sock *)sk;
}

static inline struct socket *skip_hsock(struct skip_sock *ssk)
{
	return ssk->hsock;
}

static inline struct socket *skip_vsock(struct skip_sock *ssk)
{
	return ssk->vsock;
}



static int skip_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct skip_sock *ssk;

	if (!sk) {
		pr_debug("%s, NULL sk\n", __func__);
		return 0;
	}
	pr_debug("%s\n", __func__);

	ssk = skip_sk(sk);
	if (ssk->hsock)
		sock_release(ssk->hsock);
	if (ssk->vsock)
		sock_release(ssk->vsock);

	sock_orphan(sk);
	sk_refcnt_debug_release(sk);
	sock_put(sk);

	sock->sk = NULL;

	return 0;
}

static int skip_find_lwtstate(struct socket *sock, struct sockaddr *daddr,
			      struct skip_lwt *slwtp)
{
	/* find skip lwtunnel state */

	int ret = 0;
	struct flowi4 fl4;
	struct flowi6 fl6;
	struct rtable *rt;
	struct dst_entry *dst = NULL;
	struct skip_lwt *slwt;

	switch (daddr->sa_family) {
	case AF_INET:
		memset(&fl4, 0, sizeof(fl4));
		fl4.daddr = ((struct sockaddr_in *)daddr)->sin_addr.s_addr;
		fl4.saddr = 0;	/* XXX: ? */
		rt = ip_route_output_key(sock_net(sock->sk), &fl4);
		if (IS_ERR(rt)) {
			pr_debug("%s: no route found for %pI4\n",
				 __func__, &fl4.daddr);
			ret = -ENOENT;
			goto out;
		}
		if (rt->dst.lwtstate == NULL ||
		    rt->dst.lwtstate->type != LWTUNNEL_ENCAP_SKIP) {
			pr_debug("%s: route to %pI4 is not skip\n",
				 __func__, &fl4.daddr);
			ret = -ENONET;
			goto dst_release_out;
		}

		dst = &rt->dst;
		break;

	case AF_INET6:
		memset(&fl6, 0, sizeof(fl6));
		fl6.daddr = ((struct sockaddr_in6 *)daddr)->sin6_addr;
		dst = ip6_route_output(sock_net(sock->sk), sock->sk, &fl6);
		if (dst->error) {
			pr_debug("%s: no route found for %pI6\n",
				 __func__, &fl6.daddr);
			ret = -ENOENT;
			goto out;
		}
		if (dst->lwtstate == NULL ||
		    dst->lwtstate->type != LWTUNNEL_ENCAP_SKIP) {
			pr_debug("%s: route to %pI6 is not skip\n",
				 __func__, &fl6.daddr);
			ret = -ENONET;
			goto dst_release_out;
		}

		break;

	default :
		pr_err("%s: address family '%u' does not supported\n",
		       __func__, daddr->sa_family);
		return -EAFNOSUPPORT;
	}

	slwt = skip_lwt_lwtunnel(dst->lwtstate);
	*slwtp = *slwt;

dst_release_out:
	dst_release(dst);
out:
	return ret;
}

static int skip_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	int ret, h_addrlen;
	struct skip_lwt slwt;
	struct skip_sock *ssk = skip_sk(sock->sk);
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	struct sockaddr_storage saddr_s;
	struct sockaddr_in *sa4;
	struct sockaddr_in6 *sa6;
	//struct socket *vsock = skip_vsock(skip_sk(sock->sk));

	/* XXX:
	 *
	 * 1. Search routing table and find lwtunnel skip state.
	 * 2. translate uaddr and addr_len and call bind() for hsock.
	 * Then, call bind() to the socket on host netns.
	 *
	 * Consider carefully. 
	 * - how to handle INADDR_ANY? i/o multiplex is required.
	 * - bind() for virtual socket too?
	 */

	if (!uaddr)
		return -EINVAL;

	ret = skip_find_lwtstate(sock, uaddr, &slwt);
	if (ret) {
		pr_debug("%s: no skip route found\n", __func__);
		return ret;
	}

	memset(&saddr_s, 0, sizeof(saddr_s));
	switch (slwt.host_family) {
	case AF_INET:
		sa4 = (struct sockaddr_in *)&saddr_s;
		sa4->sin_family = AF_INET;
		sa4->sin_addr.s_addr = slwt.host_addr4;
		sa4->sin_port = ((struct sockaddr_in *)uaddr)->sin_port;
		h_addrlen = sizeof(struct sockaddr_in);
		break;

	case AF_INET6:
		sa6 = (struct sockaddr_in6 *)&saddr_s;
		sa6->sin6_family = AF_INET6;
		sa6->sin6_addr = slwt.host_addr6;
		sa6->sin6_port = ((struct sockaddr_in6 *)uaddr)->sin6_port;
		h_addrlen = sizeof(struct sockaddr_in6);
		break;

	default :
		pr_debug("%s: invalid family '%u' of skip route\n",
			 __func__, slwt.host_family);
		return -EAFNOSUPPORT;
	}


	ret = hsock->ops->bind(hsock, (struct sockaddr *)&saddr_s, h_addrlen);
	if (ret) {
		pr_debug("%s: hsock->ops->bind() failed, ret=%d\n",
			 __func__, ret);
		return ret;
	}

	pr_debug("%s: bind success\n", __func__);
	ssk->bound = true;	/* this socket is already bind()ed */

	return 0;
}

static int skip_connect(struct socket *sock, struct sockaddr *vaddr,
			int sockaddr_len, int flags)
{
	int ret;
	int h_addrlen;
	struct skip_sock *ssk = skip_sk(sock->sk);
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	struct sockaddr_storage saddr_s;

	/* XXX: bind() should be called for vsock? */

	if (sock->sk->sk_protocol == IPPROTO_UDP && !ssk->bound) {
		/* bind(port 0) before connect() for SOCK_DRGAM
		 * sockets to prevent container from using addresses
		 * not assinged to this container */
		memcpy(&saddr_s, vaddr, sockaddr_len);
		switch (saddr_s.ss_family){
		case AF_INET:
			((struct sockaddr_in*)&saddr_s)->sin_port = 0;
			h_addrlen = sizeof(struct sockaddr_in);
			break;
		case AF_INET6:
			((struct sockaddr_in6*)&saddr_s)->sin6_port = 0;
			h_addrlen = sizeof(struct sockaddr_in6);
			break;
		default:
			pr_debug("%s: invalid family '%u' of skip route\n",
				 __func__, saddr_s.ss_family);
			return -EAFNOSUPPORT;
		}
		ret = hsock->ops->bind(hsock, (struct sockaddr *)&saddr_s,
				       h_addrlen);
		if (ret) {
			pr_debug("%s: bind() before connect failed '%d'\n",
				 __func__, ret);
			return ret;
		}
	}

	return hsock->ops->connect(hsock, vaddr, sockaddr_len, flags);
}

static int skip_socketpair(struct socket *sock1, struct socket *sock2)
{
	/* XXX: ??? */

	struct socket *hsock = skip_hsock(skip_sk(sock1->sk));
	return hsock->ops->socketpair(hsock, sock2);
}

static int skip_accept(struct socket *sock, struct socket *newsocket,
		       int flags)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));	
	return hsock->ops->accept(hsock, newsocket, flags);
}

static int skip_getname(struct socket *sock, struct sockaddr *addr,
			int *sockaddr_len, int peer)
{
	/* XXX: getname should be executed on vsock? */

	struct socket *hsock = skip_hsock(skip_sk(sock->sk));	
	return hsock->ops->getname(hsock, addr, sockaddr_len, peer);
}

static unsigned int skip_poll(struct file *file, struct socket *sock,
			      struct poll_table_struct *wait)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));	
	return hsock->ops->poll(file, hsock, wait);
}


static int skip_ioctl(struct socket *sock, unsigned int cmd,
		      unsigned long arg)
{
	/* XXX: ioctl should be executed on both h/vsock? */

	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	return hsock->ops->ioctl(hsock, cmd, arg);
}

static int skip_listen(struct socket *sock, int len)
{
	/* XXX: ioctl should be executed on both h/vsock? */

	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	return hsock->ops->listen(hsock, len);
}


static int skip_shutdown(struct socket *sock, int flags)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));

	/* XXX:
	 * shutdown() is called for socket accept()ed.
	 * accept() sockets do not have virtual socket on netns.
	 * Thus, in this function, only hsock->ops->shutdown is called.
	 */
	return hsock->ops->shutdown(hsock, flags);
}

static int skip_setsockopt(struct socket *sock, int level,
			   int optname, char __user *optval,
			   unsigned int optlen)
{
	/* XXX: setsockopt should be executed on both h/vsock? */

	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	return hsock->ops->setsockopt(hsock, level, optname, optval, optlen);
}

static int skip_getsockopt(struct socket *sock, int level,
			   int optname, char __user *optval,
			   int __user * optlen)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	return hsock->ops->getsockopt(hsock, level, optname, optval, optlen);
}

static int skip_sendmsg(struct socket *sock,
			struct msghdr *m, size_t total_len)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	
	/* XXX: impliment bind() before connect()/send*() !! */
	return hsock->ops->sendmsg(hsock, m, total_len);
}

static int skip_recvmsg(struct socket *sock,
			struct msghdr *m, size_t total_len, int flags)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	return hsock->ops->recvmsg(hsock, m, total_len, flags);
}

static ssize_t skip_sendpage(struct socket *sock, struct page *page,
			     int offset, size_t size, int flags)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	return hsock->ops->sendpage(hsock, page, offset, size, flags);
}


static ssize_t skip_splice_read(struct socket *sock, loff_t *ppos,
			       struct pipe_inode_info *pipe,
			       size_t len, unsigned int flags)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	return hsock->ops->splice_read(hsock, ppos, pipe, len, flags);
}

static int skip_set_peek_off(struct sock *sk, int val)
{
	/* XXX: set_peek_off should be executed on both h/vsock? */

	struct socket *hsock = skip_hsock(skip_sk(sk));
	return hsock->ops->set_peek_off(hsock->sk, val);
}

static const struct proto_ops skip_proto_ops = {
	.family		= PF_SKIP,
	.owner		= THIS_MODULE,
	.release	= skip_release,
	.bind		= skip_bind,
	.connect	= skip_connect,
	.socketpair	= skip_socketpair,
	.accept		= skip_accept,
	.getname	= skip_getname,
	.poll		= skip_poll,
	.ioctl		= skip_ioctl,
	.listen		= skip_listen,
	.shutdown	= skip_shutdown,
	.setsockopt	= skip_setsockopt,
	.getsockopt	= skip_getsockopt,
	.sendmsg	= skip_sendmsg,
	.recvmsg	= skip_recvmsg,
	.mmap		= sock_no_mmap,
	.sendpage	= skip_sendpage,
	.splice_read	= skip_splice_read,
	.set_peek_off	= skip_set_peek_off,
};

static struct proto skip_proto = {
	.name		= "SKIP",
	.owner		= THIS_MODULE,
	.obj_size	= sizeof(struct skip_sock),
};

static int skip_create(struct net *net, struct socket *sock,
		       int protocol, int kern)
{
	int ret;
	struct sock *sk;
	struct skip_sock *ssk;

	pr_debug("%s\n", __func__);

	sock->ops = &skip_proto_ops;

	sk = sk_alloc(net, PF_SKIP, GFP_KERNEL, &skip_proto, kern);
	if (!sk)
		return -ENOMEM;

	sock_init_data(sock, sk);

	ssk = skip_sk(sk);
	ssk->sock = sock;
	ssk->bound = false;

	/* XXX:
	 * 
	 * actual sockets (on both netns and defualt netns) are
	 * created when any one of bind(), connect(), sendto/msg() are
	 * called.
	 */
	ret = __sock_create(get_net(&init_net),
			    AF_INET, sk->sk_type, sk->sk_protocol,
			    &ssk->hsock, kern);
	if (ret < 0) {
		pr_err("%s: failed to create a socket on default netns\n",
		       __func__);
		sk_free(sk);
		return ret;
	}

	return 0;
}


static struct net_proto_family skip_family_ops = {
	.family	= PF_SKIP,
	.create	= skip_create,
	.owner	= THIS_MODULE,
};


int af_skip_init(void)
{
	int ret;

	ret = proto_register(&skip_proto, 1);
	if (ret) {
		pr_err("%s: proto_register failed '%d'\n", __func__, ret);
		goto proto_register_failed;
	}

	ret = sock_register(&skip_family_ops);
	if (ret) {
		pr_err("%s: sock_register failed '%d'\n", __func__, ret);
		goto sock_register_failed;
	}

	return ret;

sock_register_failed:
	proto_unregister(&skip_proto);
proto_register_failed:
	return ret;
}


void af_skip_exit(void)
{
	sock_unregister(PF_SKIP);
	proto_unregister(&skip_proto);
}
