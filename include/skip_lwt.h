/* skip_wlt.h - SKIP interface */

#ifndef _SKIP_LWT_H_
#define _SKIP_LWT_H_


#ifdef __KERNEL__

#include <net/lwtunnel.h>

/* skip lwtunnel state structure */
struct skip_lwt {
	int		dst_family;
	__be32		dst_addr4;
	struct in6_addr dst_addr6;

	int		host_family;
	__be32		host_addr4;
	struct in6_addr	host_addr6;

	bool inbound;
	bool outbound;

	bool v4v6map;
	struct in6_addr map_prefix;
};

static inline struct skip_lwt *skip_lwt_lwtunnel(struct lwtunnel_state *lwt)
{
        return (struct skip_lwt *)lwt->data;
}

#endif	/* __KERNEL__ */



/* XXX: overwrite ILA encap type by skip
 * because lwtunnel_encap_types is defined as enum... */
#define LWTUNNEL_ENCAP_SKIP     LWTUNNEL_ENCAP_ILA

enum {
	SKIP_ATTR_UNSPEC,

	SKIP_ATTR_HOST_ADDR4,		/* be32 */
	SKIP_ATTR_HOST_ADDR6,		/* binary 128bit */
	SKIP_ATTR_HOST_ADDR_FAMILY,	/* u32 */

	SKIP_ATTR_INBOUND,		/* u8: true 1, false 0 */
	SKIP_ATTR_OUTBOUND,		/* u8: true 1, false 0 */

	SKIP_ATTR_MAP_V4V6,		/* u8: true 1, false 0 */
	SKIP_ATTR_MAP_PREFIX,		/* binary 128bit */

	__SKIP_ATTR_MAX,
};

#define SKIP_ATTR_MAX	(__SKIP_ATTR_MAX - 1)


#endif
