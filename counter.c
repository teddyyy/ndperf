#include "ndperf.h"
#include "counter.h"

static
struct flow_counter * init_flow_counter(struct in6_addr *addr)
{
        struct flow_counter *fc;

        fc = malloc(sizeof(struct flow_counter));
	if (fc == NULL) {
		perror("malloc");
		return NULL;
	}

	inet_ntop(AF_INET6, addr, fc->addr_str, sizeof(fc->addr_str));
        fc->sent = 0;
        fc->received = 0;

        return fc;
}

struct fc_ptr *
setup_flow_counter(struct in6_addr *addr, int node_num)
{
	struct fc_ptr *fcp;
	struct in6_addr dstaddr;

        fcp = (struct fc_ptr*)malloc(sizeof(*fcp));
	if (fcp == NULL) {
		perror("malloc");
		return NULL;
	}

	memcpy(&dstaddr, addr, sizeof(struct in6_addr));

        // initialize hash table
        init_flow_hash();

        fcp->keys = malloc(sizeof(struct in6_addr) * node_num);
	if (fcp->keys == NULL) {
		perror("malloc");
		return NULL;
	}

        for (int i = 0; i < node_num; i++) {
		// create key
                increment_ipv6addr_plus_one(&dstaddr);
		fcp->keys[i] = dstaddr;

		// create value
                fcp->val = init_flow_counter(&dstaddr);
		if (fcp->val == NULL) {
			fprintf(stderr, "Unable to initialize flow counter\n");
			return NULL;
		}

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
