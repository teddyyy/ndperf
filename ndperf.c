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

#define SRCADDR	"2001:2:0:0::1"
#define DSTADDR	"2001:2:0:1::1"
#define TXIF	"enp0s3"

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
build_ipv6_hdr(char *pkt, char *srcip, char *dstip)
{
	struct ip6_hdr *ip6h;
	struct in6_addr saddr, daddr;

	ip6h = (struct ip6_hdr*)pkt;

	ip6h->ip6_flow = 0x60; // set ipv6 version
	ip6h->ip6_plen = 0; // no payload
	ip6h->ip6_nxt = 59; // no next header
	ip6h->ip6_hlim = 255;

	inet_pton(AF_INET6, srcip, &saddr);
	memcpy(&ip6h->ip6_src, &saddr, sizeof(saddr));
	inet_pton(AF_INET6, dstip, &daddr);
	memcpy(&ip6h->ip6_dst, &daddr, sizeof(daddr));

	return;
}

static bool expired = false;

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

int
main(int argc, char *argv[])
{
	int sock, len, dstnum;
	char dstaddr[48] = DSTADDR;
	char srcaddr[48] = SRCADDR;
	char pkt[2048];
	struct sockaddr_in6 dst_sin;

	const struct itimerval timer = {.it_value.tv_sec = 1,
		.it_value.tv_usec = 0};
	const struct itimerval stop = {.it_value.tv_sec = 0,
		.it_value.tv_usec = 0};

	if (argc != 2) {
		fprintf(stderr, "./ndperf <destination num>\n");
		return -1;
	}

	dstnum = atoi(argv[1]);

	if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("socket");
		return -1;
	}

	// bind specific interface
	setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, TXIF, strlen(TXIF) + 1);

	set_signal(SIGALRM);

	// send packet until timer expires
	for (int i = 1; i <= dstnum; i++) {

		// set timeout
		setitimer(ITIMER_REAL, &timer, 0);

		for (int j = 1; j <= i; j++) {

			if (expired)
				break;

			increment_string_ipv6addr(dstaddr, sizeof(dstaddr));
			build_ipv6_hdr(pkt, srcaddr, dstaddr);

			dst_sin.sin6_family = AF_INET6;
			if (inet_pton(AF_INET6, dstaddr,
				      &(dst_sin.sin6_addr)) != 1) {
				perror("inet_pton");
				return -1;
			}

			if ((len = sendto(sock, (void *)pkt,
					  sizeof(struct ip6_hdr), 0,
					  (struct sockaddr *)&dst_sin,
					  sizeof(dst_sin))) == -1) {
				perror("sendto");
				return -1;
			}

			// reset increment
			if (j == i) {
				j = 0;
				strcpy(dstaddr, DSTADDR);
			}

			usleep(100000);
		}

		// stop timeout
		setitimer(ITIMER_REAL, &stop, 0);
		expired = false;

	}

	return 0;
}
