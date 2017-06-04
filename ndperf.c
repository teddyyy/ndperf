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

	ip6h->ip6_flow = 0x60;	// set IPv6 version
	ip6h->ip6_plen = 0;	// no payload
	ip6h->ip6_nxt = 59;	// no next header
	ip6h->ip6_hlim = 255;
	ip6h->ip6_src = *src;
	ip6h->ip6_dst = *dst;
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

static int
init_tx_socket(char *ifn)
{
	int sock;

	if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("socket");
		return -1;
	}

	// bind specific interface
	setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifn, strlen(ifn) + 1);

	return sock;
}

static void
usage(char *prgname)
{
	fprintf(stderr, "usage:\t%s\n", prgname);
	fprintf(stderr, "\t-i: interface\n");
	fprintf(stderr, "\t-s: source IPv6 address\n");
	fprintf(stderr, "\t-d: destination IPv6 address\n");
	fprintf(stderr, "\t-n: neighbor number\n");
	fprintf(stderr, "\t-h: prints this help text\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\te.g: sudo ./ndperf -i enp0s3 -s 2001:2:0:0::1 -d 2001:2:0:1::1 -n 3\n");

	exit(0);
}

int
main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in6 dst;
	char pkt[2048];

	// for option
	int  option, node_num = 0;
	struct in6_addr srcaddr, dstaddr, start_addr;
	char *prgname = 0, *ifname = 0;

	prgname = argv[0];

	while ((option = getopt(argc, argv, "hi:s:d:n:")) > 0) {
		switch(option) {
		case 'h':
			usage(prgname);
			break;
		case 'i':
			ifname = optarg;
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

			if (inet_pton(AF_INET6, optarg, &start_addr) != 1) {
				fprintf(stderr, "cannot convert dst address\n");
				usage(prgname);
			}

			break;
		case 'n':
			node_num = atoi(optarg);
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

	if (ifname == 0)
		usage(prgname);

	if (node_num < 1)
		usage(prgname);

	// setup tx
	if ((sock = init_tx_socket(ifname)) < 0) {
		fprintf(stderr, "cannot initialize tx socket\n");
		return -1;
	}

	set_signal(SIGALRM);

	const struct itimerval timer = {
		.it_value.tv_sec = DEFAULT_TIMER,
		.it_value.tv_usec = 0
	};

	const struct itimerval stop = {
		.it_value.tv_sec = 0,
		.it_value.tv_usec = 0
	};

	/*
	 * tx loop
	 */
	for (int i = 1; i <= node_num; i++) {
		// setup process
		struct fc_ptr *fcp = setup_flow_counter(&dstaddr, i);
		dstaddr = start_addr;

		// set timeout
		setitimer(ITIMER_REAL, &timer, 0);

		for (int count = 1; count <= i; count++) {

			// send packet until timer expires
			if (expired)
				break;

			increment_ipv6addr_plus_one(&dstaddr);
			build_ipv6_pkt(pkt, &srcaddr, &dstaddr);

			dst.sin6_family = AF_INET6;
			dst.sin6_addr = dstaddr;

			if (sendto(sock, (void *)pkt, sizeof(struct ip6_hdr), 0,
				   (struct sockaddr *)&dst, sizeof(dst)) != -1) {
				countup_val_flow_hash(&dstaddr, HASH_TX);
			}

			// stop increment, from begining
			if (count == i) {
				count = 0;
				dstaddr = start_addr;
			}

			usleep(50000);
		}

		// display stats of flow
		print_flow_hash(i);

		// stop timeout
		setitimer(ITIMER_REAL, &stop, 0);
		expired = false;

		// clean process
		cleanup_flow_counter(fcp);
		dstaddr = start_addr;
	}

	return 0;
}
