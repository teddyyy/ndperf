#include "ndperf.h"
#include "counter.h"

static
struct flow_counter * init_flow_counter()
{
        struct flow_counter *fc;

        fc = malloc(sizeof(struct flow_counter));
        fc->sent = 0;
        fc->received = 0;

        return fc;
}

struct fc_ptr *
setup_flow_counter(char *dstaddr, int node_num)
{
        struct fc_ptr *fcp = (struct fc_ptr*)malloc(sizeof(*fcp));

        char addr[ADDRLEN];
        strcpy(addr, dstaddr);

        // initialize hash table
        init_flow_hash();
        fcp->keys = malloc(sizeof(char) * node_num * ADDRLEN);

        for (int i = 0; i < node_num; i++) {
                fcp->val = init_flow_counter();

                increment_string_ipv6addr(addr, sizeof(addr));
                strcpy(fcp->keys[i], addr);

                // insert key and value
                put_key_and_val_flow_hash(fcp->keys[i], fcp->val);
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
