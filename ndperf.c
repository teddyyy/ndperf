#include "ndperf.h"

static bool expired = false;
static bool caught_signal = false;

void
increment_ipv6addr_plus_one(struct in6_addr *addr)
{
	int i;

	// increase address by one
	addr->s6_addr[15]++;
	i = 15;
	while (i > 0 && !addr->s6_addr[i--]) addr->s6_addr[i]++;
}

static double get_timeofday_sec()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec * 1e-6;
}

static inline long
get_threshold_pps_by_speed_link(int speed)
{
	long pps;

	/*
         rfc8161

         2.2.4.  Test Traffic

	 In order to avoid link congestion, test traffic is offered at a rate
         not to exceed 50% of available link bandwidth.  In order to avoid
         burstiness and buffer occupancy, every packet in the stream is
         exactly 40 bytes long (i.e., the length of an IPv6 header with no
         IPv6 payload).
         */

	// pps = link speed * to bit order / 50% / convert to byte / packet size
	return pps = speed * 1e+6 / 2 / 8 / 60;
}

static void
create_file_path(char *ifname, int ifname_len, char *path)
{
        char str1[] = "/sys/class/net/";
        strncat(str1, ifname, ifname_len);

        memcpy(path, str1, strlen(str1));

        char str2[] = "/statistics/tx_packets";
        strncat(path, str2, strlen(str2));
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
				const struct in6_addr *b)
{
	return ((a->s6_addr32[3] == b->s6_addr32[3]) &&
	        (a->s6_addr32[2] == b->s6_addr32[2]) &&
	        (a->s6_addr32[1] == b->s6_addr32[1]) &&
	        (a->s6_addr32[0] == b->s6_addr32[0]));
}

static struct in6_addr *
parse_rx_packet(unsigned char *pkt, int len, struct in6_addr *srcaddr)
{
	struct ether_header *ether;
	struct ip6_hdr *ip6;
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

	if (!addr6_eq(&ip6->ip6_src, srcaddr) || ip6->ip6_nxt != 59)
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
receive_thread(void *conf)
{
	unsigned char buf[2048];

	struct ndperf_config *nc = (struct ndperf_config *)conf;
	int sock = nc->rx_sock;
	struct in6_addr srcaddr = nc->srcaddr;

	for (;;) {
		int pktlen = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
		if (pktlen < 0) {
			perror("recvfrom");
		} else {
			struct in6_addr *dstaddr = parse_rx_packet(buf, pktlen, &srcaddr);
			if (dstaddr != NULL)
				countup_value_flow_hash(dstaddr, HASH_RX);
		}
	}
}

static void
transmit_thread(void *conf)
{
	struct ndperf_config *nc = (struct ndperf_config *)conf;

	for (int cur_node = 1; cur_node <= nc->test_index; cur_node++) {
		increment_ipv6addr_plus_one(&nc->dstaddr);

		if (tx_packet(nc->tx_sock, &nc->srcaddr, &nc->dstaddr) != -1)
			countup_value_flow_hash(&nc->dstaddr, HASH_TX);

		// stop increment, from begining
		if (cur_node == nc->test_index) {
			cur_node = 0;
			nc->dstaddr = nc->start_dstaddr;
		}

		usleep(nc->tx_interval);
	}
}


static void
cleanup_process(struct ndperf_config *nc)
{
	int ret;

	ret = delete_virtual_interface(nc->neighbor_num);
	if (ret < 0) {
                fprintf(stderr, "Unable to delete interface\n");
                exit(1);
        }

        close(nc->tx_sock);
        close(nc->rx_sock);
}

static int
cleanup_thread(pthread_t th)
{
	int err;

	err = pthread_cancel(th);
	if (err < 0) {
		perror("pthread_cancel");
		return err;
	}

	err = pthread_join(th, NULL);
	if (err < 0) {
		perror("pthread_join");
		return err;
	}

	return 0;
}

static int
process_baseline_test(struct ndperf_config *nc)
{
	pthread_t rx_thread, tx_thread;
	struct fc_ptr *fcp = NULL;
	struct in6_addr dstaddr = nc->dstaddr;
	int ret;
	long cnt1, cnt2;

	const struct itimerval timer = {
		.it_value.tv_sec = BASELINE_TEST_TIMER,
		.it_value.tv_usec = 0
	};

	// set index
	nc->test_index = 1;


	// set counter
	fcp = setup_flow_counter(&nc->dstaddr, nc->neighbor_num);
	if (fcp == NULL) {
		fprintf(stderr, "Unable to set flow counter\n");
		exit(1);
	}

	// set timeout
	setitimer(ITIMER_REAL, &timer, 0);

	if (pthread_create(&rx_thread, NULL, (void *)receive_thread,
	                  (void *)nc) != 0) {
		perror("pthread_create");
		exit(1);
	}

	if (pthread_create(&tx_thread, NULL, (void *)transmit_thread,
	                  (void *)nc) != 0) {
		perror("pthread_create");
		exit(1);
	}

	for (;;) {
		if (expired || caught_signal)
			break;

		cnt1 = get_packet_count(nc->tx_pkt_statics_path);

		usleep(1000000);

		cnt2 = get_packet_count(nc->tx_pkt_statics_path);

		if (nc->threshold_pps < (cnt2 - cnt1))
			printf("warning: %ld pps\n", cnt2 - cnt1);
	}

	increment_ipv6addr_plus_one(&dstaddr);

	if (!is_equal_received_flow_hash(&dstaddr) || caught_signal)
		printf("Baseline test is not passed\n");
	else
		printf("Baseline test is passed\n");

	// display stats of flow
	print_flow_hash();

	if ((ret = cleanup_thread(rx_thread)) < 0) {
		fprintf(stderr, "Unable to cleanup rx thread\n");
		cleanup_flow_counter(fcp);
		return ret;
	}

	if ((ret = cleanup_thread(tx_thread)) < 0) {
		fprintf(stderr, "Unable to cleanup tx thread\n");
		cleanup_flow_counter(fcp);
		return ret;
	}

	cleanup_flow_counter(fcp);

	return 0;
}

static int
process_scaling_test(struct ndperf_config *nc)
{
	pthread_t rx_thread, tx_thread;
	struct fc_ptr *fcp = NULL;
	double start, end, duration;
	int ret;
	long cnt1, cnt2;

	const struct itimerval timer = {
		.it_value.tv_sec = SCALING_TEST_TIMER,
		.it_value.tv_usec = 0
	};

	const struct itimerval stop = {
		.it_value.tv_sec = 0,
		.it_value.tv_usec = 0
	};

	for (int test_index = 1; test_index <= nc->neighbor_num; test_index++) {
		// set index
		nc->test_index = test_index;

		// set counter
		fcp = setup_flow_counter(&nc->dstaddr, test_index);
		if (fcp == NULL) {
			fprintf(stderr, "Unable to set flow counter\n");
			exit(1);
		}

		// set timeout
		setitimer(ITIMER_REAL, &timer, 0);
		start = get_timeofday_sec();

		if (pthread_create(&rx_thread, NULL, (void *)receive_thread,
		                  (void *)nc) != 0) {
			perror("pthread_create");
			exit(1);
		}

		if (pthread_create(&tx_thread, NULL, (void *)transmit_thread,
		                  (void *)nc) != 0) {
			perror("pthread_create");
			exit(1);
		}

		cnt1 = get_packet_count(nc->tx_pkt_statics_path);

		while (1) {
			if (expired || caught_signal)
				break;

			if (is_received_flow_hash())
				break;
		}

		cnt2 = get_packet_count(nc->tx_pkt_statics_path);

		// stop timeout
		setitimer(ITIMER_REAL, &stop, 0);
		expired = false;
		end = get_timeofday_sec();

		duration = end - start;

		if ((ret = cleanup_thread(rx_thread)) < 0) {
			fprintf(stderr, "Unable to cleanup rx thread\n");
			cleanup_flow_counter(fcp);
			return ret;
		}

		if ((ret = cleanup_thread(tx_thread)) < 0) {
			fprintf(stderr, "Unable to cleanup tx thread\n");
			cleanup_flow_counter(fcp);
			return ret;
		}

		if ((nc->threshold_pps * duration) < (cnt2 - cnt1))
			printf("Warning: %ld pps. Threshold is %ld pps\n",
				cnt2 - cnt1, nc->threshold_pps);

		// failed case
		if (!is_received_flow_hash()) {
			printf("Test of %d neighbor isn't passed. It took %f seconds.\n",
				test_index, duration);
			cleanup_flow_counter(fcp);
			return -1;
		}

		if (nc->verbose)
			printf("Test of %d neighbor is passed. It took %f seconds. Current pps is %ld.\n",
				test_index, end - start,  cnt2 - cnt1);

		cleanup_flow_counter(fcp);
		nc->dstaddr = nc->start_dstaddr;
	}

	return 0;
}

static void
interrupt(int sig)
{
	caught_signal = true;
}

static void
set_interrupt_signal(int sig)
{
	if (signal(sig, interrupt) == SIG_ERR) {
		perror("signal");
		exit(1);
	}
}

static void
timeout(int sig)
{
	expired  = true;
}

static void
set_timeout_signal(int sig)
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
	fprintf(stderr, "\t-B: Baseline test mode\n");
	fprintf(stderr, "\t-S: Scaling test mode\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-i: interface(source)\n");
	fprintf(stderr, "\t-r: interface(destination)\n");
	fprintf(stderr, "\t-s: source IPv6 address\n");
	fprintf(stderr, "\t-d: destination IPv6 address\n");
	fprintf(stderr, "\t-p: destination IPv6 prefix length\n");
	fprintf(stderr, "\t-n: neighbor number (default:1)\n");
	fprintf(stderr, "\t-I: transmit interval(μs) (default:100μs)\n");
	fprintf(stderr, "\t-v: verbose output\n");
	fprintf(stderr, "\t-h: prints this help text\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\te.g: sudo ./ndperf -B -i enp0s3 -r enp0s8 -s 2001:2:0:0::1 -d 2001:2:0:1::1 -p 64\n");
	fprintf(stderr, "\te.g: sudo ./ndperf -S -n 1000 -i enp0s3 -r enp0s8 -s 2001:2:0:0::1 -d 2001:2:0:1::1 -p 64\n");

	exit(0);
}

static void
init_ndperf_config(struct ndperf_config *nc)
{
	nc->tx_sock = 0;
	nc->rx_sock = 0;
	nc->mode = 0;
	nc->neighbor_num = 0;
	nc->test_index = 0;
	nc->prefixlen = 0;
	nc->verbose = false;
	nc->threshold_pps = 0;
	nc->tx_interval = -1;
	memset(&nc->srcaddr, 0, sizeof(nc->srcaddr));
	memset(&nc->dstaddr, 0, sizeof(nc->dstaddr));
	memset(&nc->start_dstaddr, 0, sizeof(nc->start_dstaddr));
	memset(&nc->tx_pkt_statics_path, 0, sizeof(nc->tx_pkt_statics_path));
}

int
main(int argc, char *argv[])
{
	char *prgname = 0;
	struct ndperf_config conf;

	// for option
	int option, link_speed;
	char *tx_if = 0, *rx_if = 0;

	prgname = argv[0];

	init_ndperf_config(&conf);

	while ((option = getopt(argc, argv, "hSBvi:s:d:p:n:t:r:I:")) > 0) {
		switch(option) {
		case 'B':
			conf.mode = BASELINE_TEST_MODE;
			break;
		case 'S':
			conf.mode = SCALING_TEST_MODE;
			break;
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
			if (inet_pton(AF_INET6, optarg, &conf.srcaddr) != 1) {
				fprintf(stderr, "Unable to convert src address\n");
				usage(prgname);
			}

			break;
		case 'd':
			if (inet_pton(AF_INET6, optarg, &conf.dstaddr) != 1) {
				fprintf(stderr, "Unable to convert dst address\n");
				usage(prgname);
			}

			if (inet_pton(AF_INET6, optarg, &conf.start_dstaddr) != 1) {
				fprintf(stderr, "Unable to convert dst address\n");
				usage(prgname);
			}

			break;
		case 'p':
			conf.prefixlen = atoi(optarg);
			break;
		case 'n':
			conf.neighbor_num = atoi(optarg);
			break;
		case 'I':
			conf.tx_interval = atoi(optarg);
			break;
		case 'v':
			conf.verbose = true;
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

	if (conf.mode == 0)
		conf.mode = BASELINE_TEST_MODE;

	if (tx_if == 0 || rx_if == 0)
		usage(prgname);

	if (conf.prefixlen < 1 || conf.prefixlen > 128)
		usage(prgname);

	if (conf.mode == BASELINE_TEST_MODE)
		conf.neighbor_num = DEFAULT_NEIGHBOR_NUM;

	if (conf.neighbor_num < 1 || conf.neighbor_num >= MAX_NODE_NUMBER)
		conf.neighbor_num = DEFAULT_NEIGHBOR_NUM;

	if (conf.tx_interval < 0)
		conf.tx_interval = DEFAULT_INTERVAL;

	if ((link_speed = get_tx_link_speed(tx_if)) < 0) {
		fprintf(stderr, "Unable to get tx link speed\n");
		return -1;
	}

	conf.threshold_pps = get_threshold_pps_by_speed_link(link_speed);
	if (conf.verbose)
		printf("Threshold pps is %ld\n", conf.threshold_pps);

	create_file_path(tx_if, strlen(tx_if), conf.tx_pkt_statics_path);

	// setup tx
	if ((conf.tx_sock = init_tx_socket(tx_if)) < 0) {
		fprintf(stderr, "Unable to initialize tx socket\n");
		return -1;
	}

	// setup rx
	if (create_virtual_interface(&conf.dstaddr, conf.prefixlen,
				     conf.neighbor_num, rx_if) < 0) {
		fprintf(stderr, "Unable to create interface\n");
		return -1;
	}

	if ((conf.rx_sock = init_rx_socket(conf.neighbor_num)) < 0) {
		fprintf(stderr, "Unable to initialize rx socket\n");
		return -1;
	}

	set_interrupt_signal(SIGINT);
	set_timeout_signal(SIGALRM);

	/* main process */
	if (conf.mode == BASELINE_TEST_MODE) {
		conf.neighbor_num = 1;

		printf("Started prewarming test...\n\n");

		if (process_scaling_test(&conf) < 0) {
			cleanup_process(&conf);
			return -1;
		}

		printf("\nStarted baseline test for %d seconds...\n\n", BASELINE_TEST_TIMER);

		if (process_baseline_test(&conf) < 0) {
			cleanup_process(&conf);
			return -1;
		}
	} else if (conf.mode == SCALING_TEST_MODE) {
		printf("Started scaling test...\n\n");
		if (process_scaling_test(&conf) < 0) {
			printf("%d neighbor test isn't passed.\n", conf.neighbor_num);
			cleanup_process(&conf);
			return -1;
		}
		printf("%d neighbor test is passed.\n", conf.neighbor_num);
	}

	cleanup_process(&conf);

	return 0;
}
