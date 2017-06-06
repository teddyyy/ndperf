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
print_pkt_header(unsigned char *pkt)
{
	struct ip6_hdr *ip6;
	char buf[80];

	pkt += sizeof(struct ether_header);
	ip6 = (struct ip6_hdr *)pkt;

	printf("ip6_nxt=%u," ,ip6->ip6_nxt);
	printf("src=%s\t", inet_ntop(AF_INET6, &ip6->ip6_src, buf, sizeof(buf)));
	printf("dst=%s\n", inet_ntop(AF_INET6, &ip6->ip6_dst, buf, sizeof(buf)));

}

static void
rx_packet(void *p)
{
	int sock = *(int *)p;
	int pktlen;
	unsigned char buf[2048];

	printf("thread created\n");

	for (;;) {
		pktlen = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
		if (pktlen < 0) {
			perror("recv");
		} else {
			print_pkt_header(buf);
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
				fprintf(stderr, "cannot convert src address\n");
				usage(prgname);
			}

			break;
		case 'd':
			if (inet_pton(AF_INET6, optarg, &dstaddr) != 1) {
				fprintf(stderr, "cannot convert dst address\n");
				usage(prgname);
			}

			if (inet_pton(AF_INET6, optarg, &start_dstaddr) != 1) {
				fprintf(stderr, "cannot convert dst address\n");
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
		fprintf(stderr, "cannot initialize tx socket\n");
		return -1;
	}

	// setup rx
	create_virtual_interface(&dstaddr, neighbor_num, rx_if);
	rx_sock = init_rx_socket(neighbor_num);

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

		// set timeout
		setitimer(ITIMER_REAL, &timer, 0);

		// create receive thread
		if (pthread_create(&rx_thread, NULL, (void *)rx_packet,
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

	delete_virtual_interface(neighbor_num);

	close(tx_sock);
	close(rx_sock);

	return 0;
}
