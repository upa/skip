/* skip_lwt.h - SKIP interface */

#ifndef _SKIP_LWT_H_
#define _SKIP_LWT_H_

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
