#include "ndperf.h"
#include "counter.h"

static
struct flow_counter * init_flow_counter(struct in6_addr *addr)
{
        struct flow_counter *fc;

        fc = malloc(sizeof(struct flow_counter));

	inet_ntop(AF_INET6, addr, fc->addr_str, sizeof(fc->addr_str));
        fc->sent = 0;
        fc->received = 0;

        return fc;
}

struct fc_ptr *
setup_flow_counter(struct in6_addr *addr, int node_num)
{
        struct fc_ptr *fcp = (struct fc_ptr*)malloc(sizeof(*fcp));
	struct in6_addr dstaddr;
	memcpy(&dstaddr, addr, sizeof(struct in6_addr));

        // initialize hash table
        init_flow_hash();
        fcp->keys = malloc(sizeof(struct in6_addr) * node_num);

        for (int i = 0; i < node_num; i++) {
		// create key
                increment_ipv6addr_plus_one(&dstaddr);
		fcp->keys[i] = dstaddr;

		// create val
                fcp->val = init_flow_counter(&dstaddr);

                // insert key and value
                put_key_and_val_flow_hash(&fcp->keys[i], fcp->val);
        }

        return fcp;
}

void
cleanup_flow_counter(struct fc_ptr *fcp)
{
        free(fcp->val);
        free(fcp->keys);
        free(fcp);

        release_flow_hash();
}
