#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/ipv6.h>

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

int
main(int argc, char *argv[])
{
	int sock, len;
	char dstaddr[48] = "2001:2:0:1::1";
	char srcaddr[48] = "2001:2:0:0::1";
	char pkt[2048];
	struct sockaddr_in6 dst_sin;

	if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("socket");
		return -1;
	}

	increment_string_ipv6addr(dstaddr, sizeof(dstaddr));
	build_ipv6_hdr(pkt, srcaddr, dstaddr);

	dst_sin.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, dstaddr, &(dst_sin.sin6_addr)) != 1) {
		perror("inet_pton");
		return -1;
	}

	if ((len = sendto(sock, (void *)pkt, sizeof(struct ip6_hdr), 0,
			  (struct sockaddr *)&dst_sin,
			  sizeof(dst_sin))) == -1) {
		perror("sendto");
		return -1;
	}

	return 0;
}
