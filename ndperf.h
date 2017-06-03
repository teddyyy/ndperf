#ifndef _ndperf_h_
#define _ndperf_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/ipv6.h>

#include "khash.h"

#define DEFAULT_TIMER   60
#define ADDRLEN	48

#define HASH_TX 1
#define HASH_RX 2

struct flow_counter {
        unsigned long sent;
        unsigned long received;
};

typedef struct {
        char *key;
        struct flow_counter *val;
} hash_t;

struct fc_ptr {
	char (*keys)[ADDRLEN];
	struct flow_counter *val;
};

void increment_string_ipv6addr(char *addr_str, int addrlen);

/* flow */
void init_flow_hash();
void release_flow_hash();
void print_flow_hash();
void put_key_and_val_flow_hash(char *key, struct flow_counter *val);
void countup_val_flow_hash(char *key, int mode);

/* counter */
struct fc_ptr * setup_flow_counter(char *dstaddr, int node_num);
void cleanup_flow_counter(struct fc_ptr *p);

#endif
