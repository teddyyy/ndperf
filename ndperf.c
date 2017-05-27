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

#define DEFAULT_TIMER	60

static bool expired = false;

static void
increment_string_ipv6addr(char *addr_str, int addrlen)
{
	struct in6_addr addr;
	int i;

	// convert string to address
	inet_pton(AF_INET6, addr_str, &addr);

	// increase address by one
	addr.s6_addr[15]++;
	i = 15;
	while (i > 0 && !addr.s6_addr[i--]) addr.s6_addr[i]++;

	// convert address to string
	inet_ntop(AF_INET6, &addr, addr_str, addrlen);

	return;
}

static void
build_ipv6_pkt(char *pkt, char *src, char *dst)
{
	struct ip6_hdr *ip6h;
	struct in6_addr saddr, daddr;

	ip6h = (struct ip6_hdr*)pkt;

	ip6h->ip6_flow = 0x60; // set ipv6 version
	ip6h->ip6_plen = 0; // no payload
	ip6h->ip6_nxt = 59; // no next header
	ip6h->ip6_hlim = 255;

	inet_pton(AF_INET6, src, &saddr);
	memcpy(&ip6h->ip6_src, &saddr, sizeof(saddr));
	inet_pton(AF_INET6, dst, &daddr);
	memcpy(&ip6h->ip6_dst, &daddr, sizeof(daddr));

	return;
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
	fprintf(stderr, "\t-i: interface\n");
	fprintf(stderr, "\t-s: source IPv6 address\n");
	fprintf(stderr, "\t-d: destination IPv6 address\n");
	fprintf(stderr, "\t-n: neighbor number\n");
	fprintf(stderr, "\t-h: prints this help text\n");

	exit(0);
}

int
main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in6 dst_sin;
	char pkt[2048];

	// for option
	int  option, nn = 0;
	char dstaddr[48];
	char srcaddr[48];
	char *prgname = 0, *ifname = 0, *firstaddr = 0;

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
			strcpy(srcaddr, optarg);
			break;
		case 'd':
			strcpy(dstaddr, optarg);
			firstaddr = optarg;
			break;
		case 'n':
			nn = atoi(optarg);
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

	if (ifname == 0 || srcaddr == 0 || dstaddr == 0)
		usage(prgname);

	if (nn < 1)
		usage(prgname);

	if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("socket");
		return -1;
	}

	// bind specific interface
	setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifname, strlen(ifname) + 1);

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
	for (int i = 1; i <= nn; i++) {
		// set timeout
		setitimer(ITIMER_REAL, &timer, 0);

		for (int j = 1; j <= i; j++) {

			// send packet until timer expires
			if (expired)
				break;

			increment_string_ipv6addr(dstaddr, sizeof(dstaddr));
			build_ipv6_pkt(pkt, srcaddr, dstaddr);

			dst_sin.sin6_family = AF_INET6;
			inet_pton(AF_INET6, dstaddr, &(dst_sin.sin6_addr));

			if (sendto(sock, (void *)pkt, sizeof(struct ip6_hdr), 0,
				   (struct sockaddr *)&dst_sin, sizeof(dst_sin)) < 0) {
				perror("sendto");
				return -1;
			}

			// stop increment, from begining
			if (j == i) {
				j = 0;
				strcpy(dstaddr, firstaddr);
			}

			usleep(50000);
		}

		// stop timeout
		setitimer(ITIMER_REAL, &stop, 0);
		expired = false;

	}

	return 0;
}
