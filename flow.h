#ifndef _flow_h_
#define _flow_h_

#include "khash.h"

KHASH_MAP_INIT_STR(flow_hash_t, struct flow_counter *)
khash_t(flow_hash_t) *h;

#endif
