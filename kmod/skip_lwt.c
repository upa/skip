
/*
 * skip over socket processing :
 * 
 * Routing table implementation of the skip based on Linux light
 * weight tunneling infrastrcture.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/types.h>
#include <net/ip.h>
#include <uapi/linux/lwtunnel.h>
#include <net/lwtunnel.h>
#include <net/ip_fib.h>
#include <net/ip6_fib.h>

#include "skip_lwt.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt


/* XXX: overwrite ILA encap type by skip
 * because lwtunnel_encap_types is defined as enum... */
#define LWTUNNEL_ENCAP_SKIP	LWTUNNEL_ENCAP_ILA


/* skip lwtunnel state structure */
struct skip_lwt {
	int		dst_family;
	__be32		dst_addr4;
	struct in6_addr	dst_addr6;

	int 		host_family;
	__be32		host_addr4;
	struct in6_addr	host_addr6;
	
	bool inbound;
	bool outbound;
	
	bool v4v6map;
	struct in6_addr map_prefix;
};

static inline struct skip_lwt *skip_lwt_tunnel(struct lwtunnel_state *lwt)
{
	return (struct skip_lwt *)lwt->data;
}


static int skip_input(struct sk_buff *skb)
{
	/* XXX: skip (currently) never enacp any packets */
	return 0;
}

static int skip_output(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	/* XXX: skip (currently) never enacp any packets */
	return 0;
}

static const struct nla_policy skip_nl_policy[SKIP_ATTR_MAX + 1] = {
	[SKIP_ATTR_HOST_ADDR4]	= { .type = NLA_U32 },
	[SKIP_ATTR_HOST_ADDR6]	= { .type = NLA_BINARY,
				    .len = sizeof(struct in6_addr) },
	[SKIP_ATTR_HOST_ADDR_FAMILY] = { .type = NLA_U32 },
	[SKIP_ATTR_INBOUND]	= { .type = NLA_U8 },
	[SKIP_ATTR_OUTBOUND]	= { .type = NLA_U8 },
	[SKIP_ATTR_MAP_V4V6]	= { .type = NLA_U8 },
	[SKIP_ATTR_MAP_PREFIX]	= { .type = NLA_BINARY,
				    .len = sizeof(struct in6_addr) },
};

static void skip_pr_state(struct skip_lwt *slwt)
{
	pr_info("lwt: dst: family %d, addr4 %pI4, addr6 %pI6\n",
		slwt->dst_family, &slwt->dst_addr4, &slwt->dst_addr6);
	pr_info("lwt: host: family %d, addr4 %pI4, addr6 %pI6\n",
		slwt->host_family, &slwt->host_addr4, &slwt->host_addr6);
	pr_info("lwt: inoubnd %d, outbound %d\n",
		slwt->inbound, slwt->outbound);
	pr_info("lwt: v4v6map %d, map_prefix %pI6\n",
		slwt->v4v6map, &slwt->map_prefix);
}

static int skip_build_state(struct net_device * dev, struct nlattr *nla,
			    unsigned int family, const void *cfg,
			    struct lwtunnel_state **ts)
{
	int ret;
	struct skip_lwt *slwt;
	struct nlattr *tb[SKIP_ATTR_MAX + 1];
	struct lwtunnel_state *newts;
	const struct fib_config *cfg4 = cfg;
	const struct fib6_config *cfg6 = cfg;

	ret = nla_parse_nested(tb, SKIP_ATTR_MAX, nla, skip_nl_policy);
	if (ret < 0)
		return ret;

	newts = lwtunnel_state_alloc(sizeof(*slwt));
	if (!newts)
		return -ENOMEM;

	ret = -EINVAL;

	slwt = skip_lwt_tunnel(newts);
	memset(slwt, 0, sizeof(*slwt));

	slwt->dst_family = family;
	if (family == AF_INET)
		slwt->dst_addr4 = cfg4->fc_dst;
	else if (family == AF_INET6)
		slwt->dst_addr6 = cfg6->fc_dst;
	else {
		pr_err("invalid family of route '%u'", family);
		goto err_out;
	}

	/* parse and setup host address acquisition */
	if (!tb[SKIP_ATTR_HOST_ADDR_FAMILY]) {
		pr_err("SKIP_ATTR_HOST_ADDR_FAMILY does not exist\n");
		goto err_out;
	}
	slwt->host_family = nla_get_u32(tb[SKIP_ATTR_HOST_ADDR_FAMILY]);

	if (slwt->host_family == AF_INET)
		slwt->host_addr4 = nla_get_be32(tb[SKIP_ATTR_HOST_ADDR4]);
	else if (slwt->host_family == AF_INET6)
		nla_memcpy(&slwt->host_addr6, tb[SKIP_ATTR_HOST_ADDR6],
			   sizeof(struct in6_addr));
	else {
		pr_err("invalid family of host address '%u'\n",
		       slwt->host_family);
		goto err_out;
	}
			
	/* setup inbound/outbound configurations */
	if (tb[SKIP_ATTR_INBOUND] && nla_get_u8(tb[SKIP_ATTR_INBOUND]))
		slwt->inbound = true;
	if (tb[SKIP_ATTR_OUTBOUND] && nla_get_u8(tb[SKIP_ATTR_OUTBOUND]))
		slwt->outbound = true;

	/* setup v4/v6 mapping configurations */
	if (tb[SKIP_ATTR_MAP_V4V6] && tb[SKIP_ATTR_MAP_PREFIX] &&
	    nla_get_u8(tb[SKIP_ATTR_MAP_V4V6])) {
		slwt->v4v6map = true;
		nla_memcpy(&slwt->map_prefix, tb[SKIP_ATTR_MAP_PREFIX],
			   sizeof(struct in6_addr));
	}

	
	newts->type = LWTUNNEL_ENCAP_SKIP;
        newts->flags |= LWTUNNEL_STATE_OUTPUT_REDIRECT |
                        LWTUNNEL_STATE_INPUT_REDIRECT;
	pr_info("%s: dev %p\n", __func__, dev);

	/* XXX:
	 * current skip does not have any functionalities for actual
	 * output and input, so that it does not add
	 * LWTUNNEL_STATE_OUTPUT_REDIREXCT and INPUT_REDIRECT. skip is
	 * a routing entry for socket offloading.
	 */
	
	*ts = newts;

	skip_pr_state(slwt);

	return 0;

err_out:
	kfree(newts);
	*ts = NULL;
	return ret;
}

static void skip_destroy_state(struct lwtunnel_state *lwt)
{
	
}

static int skip_fill_encap_info(struct sk_buff *skb,
				struct lwtunnel_state *lwtstate)
{
	struct skip_lwt *slwt = skip_lwt_tunnel(lwtstate);

	if (nla_put_u32(skb, SKIP_ATTR_HOST_ADDR_FAMILY, slwt->host_family))
		goto nla_put_failure;

	if (slwt->dst_family == AF_INET) {
		if (nla_put_be32(skb, SKIP_ATTR_HOST_ADDR4, slwt->host_addr4))
			goto nla_put_failure;
	} else if (slwt->dst_family == AF_INET6) {
		if (nla_put(skb, SKIP_ATTR_HOST_ADDR6, sizeof(struct in6_addr),
			    &slwt->host_addr6))
			goto nla_put_failure;
	}

	if (nla_put_u8(skb, SKIP_ATTR_INBOUND, slwt->inbound ? 1 : 0))
		goto nla_put_failure;

	if (nla_put_u8(skb, SKIP_ATTR_OUTBOUND, slwt->outbound ? 1 : 0))
		goto nla_put_failure;

	if (nla_put_u8(skb, SKIP_ATTR_MAP_V4V6, slwt->v4v6map ? 1: 0))
		goto nla_put_failure;

	if (slwt->v4v6map) {
		if (nla_put(skb, SKIP_ATTR_MAP_PREFIX, sizeof(struct in6_addr),
			    &slwt->map_prefix))
			goto nla_put_failure;
	}

	return 0;

nla_put_failure:
	return -EMSGSIZE;
}

static int skip_encap_nlsize(struct lwtunnel_state *lwtstate)
{
	/* XXX: skip (currently) never enacp any packets */
	return 0;
}

static int skip_encap_cmp(struct lwtunnel_state *a, struct lwtunnel_state *b)
{
	/* XXX: skip (currently) never enacp any packets */
	return 0;
}

static const struct lwtunnel_encap_ops skip_encap_ops = {
	.build_state	= skip_build_state,
	.destroy_state	= skip_destroy_state,
	.output		= skip_output,
	.input		= skip_input,
	.fill_encap	= skip_fill_encap_info,
	.get_encap_size	= skip_encap_nlsize,
	.cmp_encap	= skip_encap_cmp,
	.owner		= THIS_MODULE,
};


int skip_lwt_init(void)
{
	return lwtunnel_encap_add_ops(&skip_encap_ops, LWTUNNEL_ENCAP_SKIP);
}

void skip_lwt_exit(void)
{
	lwtunnel_encap_del_ops(&skip_encap_ops, LWTUNNEL_ENCAP_SKIP);
}
