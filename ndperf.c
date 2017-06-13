#include "ndperf.h"

static bool expired = false;

void
increment_ipv6addr_plus_one(struct in6_addr *addr)
{
	int i;

	// increase address by one
	addr->s6_addr[15]++;
	i = 15;
	while (i > 0 && !addr->s6_addr[i--]) addr->s6_addr[i]++;
}

static void
build_ipv6_pkt(char *pkt, struct in6_addr *src, struct in6_addr *dst)
{
	struct ip6_hdr *ip6h;

	ip6h = (struct ip6_hdr*)pkt;

	ip6h->ip6_flow = 0x60;	// set protocol version
	ip6h->ip6_plen = 0;	// no payload
	ip6h->ip6_nxt = 59;	// no next header
	ip6h->ip6_hlim = 255;
	ip6h->ip6_src = *src;
	ip6h->ip6_dst = *dst;
}

static inline uint32_t addr6_eq(const struct in6_addr *a,
				const struct in6_addr *b) {
	return ((a->s6_addr32[3] == b->s6_addr32[3]) &&
	        (a->s6_addr32[2] == b->s6_addr32[2]) &&
	        (a->s6_addr32[1] == b->s6_addr32[1]) &&
	        (a->s6_addr32[0] == b->s6_addr32[0]));
}

static struct in6_addr *
parse_rx_packet(unsigned char *pkt, int len)
{
	struct ether_header *ether;
	struct ip6_hdr *ip6;
	struct in6_addr srcaddr;
	struct in6_addr *dstaddr;

	/* check packet length */
	if (len < sizeof(struct ether_header) + sizeof(struct ip6_hdr))
		return NULL;

	/* check ethernet part */
	ether = (struct ether_header *)pkt;
	if (ntohs(ether->ether_type) != ETHERTYPE_IPV6)
		return NULL;

	/* check IPv6 part */
	pkt += sizeof(struct ether_header);
	ip6 = (struct ip6_hdr *)pkt;

	inet_pton(AF_INET6, "2001:2:0:0::1", &srcaddr);

	if (!addr6_eq(&ip6->ip6_src, &srcaddr) || ip6->ip6_nxt != 59)
		return NULL;

	dstaddr = &ip6->ip6_dst;

	return  dstaddr;
}

static int
tx_packet(int sock, struct in6_addr *src, struct in6_addr *dst)
{
	char pkt[2048];
	struct sockaddr_in6 in6;

	build_ipv6_pkt(pkt, src, dst);

	memset(&in6, 0, sizeof(struct sockaddr_in6));
	in6.sin6_family = AF_INET6;
	memcpy(&in6.sin6_addr, dst, sizeof(struct in6_addr));

	int pktlen = sendto(sock, pkt, sizeof(struct ip6_hdr), 0,
			   (struct sockaddr *)&in6, sizeof(in6));

	return pktlen;
}

static void
receive_thread(void *p)
{
	int sock = *(int *)p;
	unsigned char buf[2048];
	struct in6_addr *dstaddr;

	for (;;) {
		int pktlen = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
		if (pktlen < 0) {
			perror("recvfrom");
		} else {
			dstaddr = parse_rx_packet(buf, pktlen);
			if (dstaddr != NULL)
				countup_val_flow_hash(dstaddr, HASH_RX);
		}
	}
}

static void
timeout(int sig)
{
	expired  = true;
}

static void
set_signal(int sig)
{
	if (signal(sig, timeout) == SIG_ERR) {
		perror("signal");
		exit(1);
	}
}

static void
usage(char *prgname)
{
	fprintf(stderr, "usage:\t%s\n", prgname);
	fprintf(stderr, "\t-i: interface(source)\n");
	fprintf(stderr, "\t-r: interface(destination)\n");
	fprintf(stderr, "\t-s: source IPv6 address\n");
	fprintf(stderr, "\t-d: destination IPv6 address\n");
	fprintf(stderr, "\t-n: neighbor number (1-8195)\n");
	fprintf(stderr, "\t-t: time when packet is being transmitted\n");
	fprintf(stderr, "\t-h: prints this help text\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\te.g: sudo ./ndperf -i enp0s3 -r enp0s8 -s 2001:2:0:0::1 -d 2001:2:0:1::1 -n 3 -t 60\n");

	exit(0);
}

int
main(int argc, char *argv[])
{
	char *prgname = 0;
	int tx_sock, rx_sock;
	pthread_t rx_thread;

	// for option
	int  option, neighbor_num = 0, expire_time = 0;
	struct in6_addr srcaddr, dstaddr, start_dstaddr;
	char *tx_if = 0, *rx_if = 0;

	prgname = argv[0];

	while ((option = getopt(argc, argv, "hi:s:d:n:t:r:")) > 0) {
		switch(option) {
		case 'h':
			usage(prgname);
			break;
		case 'i':
			tx_if = optarg;
			break;
		case 'r':
			rx_if = optarg;
			break;
		case 's':
			if (inet_pton(AF_INET6, optarg, &srcaddr) != 1) {
				fprintf(stderr, "Unable to convert src address\n");
				usage(prgname);
			}

			break;
		case 'd':
			if (inet_pton(AF_INET6, optarg, &dstaddr) != 1) {
				fprintf(stderr, "Unable to convert dst address\n");
				usage(prgname);
			}

			if (inet_pton(AF_INET6, optarg, &start_dstaddr) != 1) {
				fprintf(stderr, "Unable to convert dst address\n");
				usage(prgname);
			}

			break;
		case 'n':
			neighbor_num = atoi(optarg);
			break;
		case 't':
			expire_time = atoi(optarg);
			break;
		default:
			usage(prgname);
		}
	}

	argv += optind;
	argc -= optind;

	if (argc > 0) {
		fprintf(stderr, "too many options!\n");
		usage(prgname);
	}

	if (tx_if == 0 || rx_if == 0)
		usage(prgname);

	if (neighbor_num < 1 || neighbor_num >= MAX_NODE_NUMBER)
		neighbor_num = DEFAULT_NEIGHBOR_NUM;

	if (expire_time < 1)
		expire_time = DEFAULT_TIMER;

	// setup tx
	if ((tx_sock = init_tx_socket(tx_if)) < 0) {
		fprintf(stderr, "Unable to initialize tx socket\n");
		return -1;
	}

	// setup rx
	if (create_virtual_interface(&dstaddr, neighbor_num, rx_if) < 0) {
		fprintf(stderr, "Unable to create interface\n");
		return -1;
	}

	if ((rx_sock = init_rx_socket(neighbor_num)) < 0) {
		fprintf(stderr, "Unable to initialize rx socket\n");
		return -1;
	}

	set_signal(SIGALRM);

	const struct itimerval timer = {
		.it_value.tv_sec = expire_time,
		.it_value.tv_usec = 0
	};

	const struct itimerval stop = {
		.it_value.tv_sec = 0,
		.it_value.tv_usec = 0
	};

	/*
	 * main loop
	 */
	for (int node = 1; node <= neighbor_num; node++) {
		// set counter
		struct fc_ptr *fcp = setup_flow_counter(&dstaddr, node);
		if (fcp == NULL) {
			fprintf(stderr, "Unable to set flow counter\n");
			return -1;
		}

		// set timeout
		setitimer(ITIMER_REAL, &timer, 0);

		// create receive thread
		if (pthread_create(&rx_thread, NULL, (void *)receive_thread,
		                  (void *)&rx_sock) != 0) {
			perror("pthread_create");
			exit(1);
		}

		for (int count = 1; count <= node; count++) {
			// send packet until timer expires
			if (expired)
				break;

			increment_ipv6addr_plus_one(&dstaddr);

			if (tx_packet(tx_sock, &srcaddr, &dstaddr) != -1)
				countup_val_flow_hash(&dstaddr, HASH_TX);

			// stop increment, from begining
			if (count == node) {
				count = 0;
				dstaddr = start_dstaddr;
			}

			usleep(50000);
		}

		// display stats of flow
		print_flow_hash(node);

		// stop timeout
		setitimer(ITIMER_REAL, &stop, 0);
		expired = false;

		// clean process
		pthread_cancel(rx_thread);
		pthread_join(rx_thread, NULL);

		// reset process
		cleanup_flow_counter(fcp);
		dstaddr = start_dstaddr;
	}

	if ((delete_virtual_interface(neighbor_num)) < 0) {
		fprintf(stderr, "Unable to delete interface\n");
		return -1;
	}

	close(tx_sock);
	close(rx_sock);

	return 0;
}
