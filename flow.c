#include "ndperf.h"
#include "flow.h"

void
init_flow_hash()
{
        h = kh_init(flow_hash_t);
}

void
release_flow_hash()
{
        kh_destroy(flow_hash_t, h);
}

void
print_flow_hash(int node_num)
{
        struct flow_counter *val;

	printf("Neighbor number is %d\n", node_num);
        kh_foreach_value(h, val,
	{
		printf("\tDstaddr:%s\tSent:%lu\tReceived:%lu\n",
		       val->addr_str, val->sent, val->received));
	};
}

void
put_key_and_val_flow_hash(struct in6_addr *key, struct flow_counter *val)
{
        int ret;
        khiter_t k;

        k = kh_put(flow_hash_t, h, addr6_hash(key), &ret);
        if (!ret)
                kh_del(flow_hash_t, h, k);
        else
                kh_value(h, k) = val;
}

void
countup_val_flow_hash(struct in6_addr *key, int mode)
{
        struct flow_counter *val;
        khiter_t k;

        k = kh_get(flow_hash_t, h, addr6_hash(key));
        if (k != kh_end(h)) {
                val = kh_value(h, k);

		if (mode == HASH_TX)
			val->sent++;
		else if (mode == HASH_RX)
			val->received++;

		kh_value(h, k) = val;
	}
}
