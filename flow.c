#include "ndperf.h"
#include "flow.h"

void
init_flow_hash()
{
        h = kh_init(flow_hash_t);

        return;
}

void
release_flow_hash()
{
        kh_destroy(flow_hash_t, h);

        return;
}

void
print_flow_hash()
{
        const char *k;
        struct flow_counter *v;

        kh_foreach(h, k, v, printf("%s %lu %lu ", k, v->sent, v->received));
	printf("\n");
}

void
put_flow_hash(char *key, struct flow_counter *val)
{
        int ret;
        khiter_t k;

        k = kh_put(flow_hash_t, h, key, &ret);
        if (!ret)
                kh_del(flow_hash_t, h, k);
        else
                kh_value(h, k) = val;

        return;
}

void
countup_flow_hash(char *key, int mode)
{
        khiter_t k;
        struct flow_counter *val;

        k = kh_get(flow_hash_t, h, key);
        if (k != kh_end(h))
                val = kh_value(h, k);
        else
                return;

        if (mode == HASH_TX)
                val->sent++;

        if (mode == HASH_RX)
                val->received++;

        kh_value(h, k) = val;

        return;
}
