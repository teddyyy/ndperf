#ifndef _flow_h_
#define _flow_h_

#include "khash.h"

KHASH_MAP_INIT_INT(flow_hash_t, struct flow_counter *)
khash_t(flow_hash_t) *h;


/* cited by https://github.com/sora/pcappriv */
static inline uint32_t addr6_hash(const struct in6_addr *a) {
	return (a->s6_addr32[3] ^ a->s6_addr32[2] ^
	        a->s6_addr32[1] ^ a->s6_addr32[0]);
}

#endif
