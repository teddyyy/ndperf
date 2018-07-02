#ifndef _ndperf_h_
#define _ndperf_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <linux/ipv6.h>

#include "khash.h"

#define SCALING_TEST_TIMER	60
#define BASELINE_TEST_TIMER	1800

#define DEFAULT_NEIGHBOR_NUM	1
#define MAX_NODE_NUMBER		8196

#define DEFAULT_INTERVAL	100

#define	ADDRSTRLEN		48
#define HASH_TX			1
#define HASH_RX			2

#define	BASELINE_TEST_MODE	1
#define	SCALING_TEST_MODE	2

struct flow_counter {
	char addr_str[ADDRSTRLEN];
        unsigned long sent;
        unsigned long received;
};

typedef struct {
        struct in6_addr *key;
        struct flow_counter *val;
} hash_t;

struct fc_ptr {
	struct in6_addr (*keys);
	struct flow_counter *val;
};

struct ndperf_config {
        int tx_sock;
        int rx_sock;

        int mode;
        int neighbor_num;
        int test_index;

	bool verbose;

        struct in6_addr srcaddr;
        struct in6_addr dstaddr;
        struct in6_addr start_dstaddr;
        int prefixlen;

	long threshold_pps;
	long tx_interval;

	char tx_pkt_statics_path[256];
};

void increment_ipv6addr_plus_one(struct in6_addr *addr);

/* flow */
void init_flow_hash();
void release_flow_hash();
void print_flow_hash();
void put_key_and_value_flow_hash(struct in6_addr *key, struct flow_counter *val);
void countup_value_flow_hash(struct in6_addr *key, int mode);
bool is_received_flow_hash();
bool is_equal_received_flow_hash(struct in6_addr *key);

/* counter */
struct fc_ptr * setup_flow_counter(struct in6_addr *addr, int node_num);
void cleanup_flow_counter(struct fc_ptr *p);

/* interface */
int create_virtual_interface(struct in6_addr *addr, int prefixlen, int dstnum, char *pif);
int delete_virtual_interface(int ifnum);
int create_virtual_interface_name(char *ifname, int ifname_len, int num);

/* socket */
int init_tx_socket(char *ifname);
int init_rx_socket(int ifnum);

/* util */
int get_tx_link_speed(char *ifname);
long get_packet_count(char *filepath);
void increment_ipv6addr_plus_one(struct in6_addr *addr);


#endif
