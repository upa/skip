
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
#include <af_skip.h>

#include "skip.h"


#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt



struct skip_sock {
	struct sock sk;

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

	if (!sk)
		return 0;

	ssk = skip_sk(sk);
	if (ssk->hsock)
		ssk->hsock->ops->release(ssk->hsock);
	if (ssk->vsock)
		ssk->vsock->ops->release(ssk->vsock);

	sock->sk = NULL;

	return 0;
}

static int skip_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));	
	//struct socket *vsock = skip_vsock(skip_sk(sock->sk));	

	/* XXX: 
	 * 
	 * 1. Search routing table and find lwtunnel skip state.
	 * 2. Find acquired IP address on host netns.
	 * 3. translate uaddr and addr_len and call bind() for hsock.
	 * 
	 * Consider carefully. also bind() for virtual socket.
	 */

	return hsock->ops->bind(hsock, uaddr, addr_len);
}

static int skip_connect(struct socket *sock, struct sockaddr *vaddr,
			int sockaddr_len, int flags)
{
	struct socket *hsock = skip_hsock(skip_sk(sock->sk));
	
	/* XXX: impliment bind() before connect()/send*() !! */
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

	sock->ops = &skip_proto_ops;

	sk = sk_alloc(net, PF_SKIP, GFP_KERNEL, &skip_proto, kern);
	if (!sk)
		return -ENOMEM;

	sock_init_data(sock, sk);

	ssk = skip_sk(sk);
	ssk->sock = sock;

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
