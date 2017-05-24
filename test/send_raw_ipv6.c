#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/ipv6.h>

static void
create_string_ipv6addr(char *ipv6addr, int addr_len, int num)
{
	char endaddr[8] = "";

	//Since .1 addr is used by DUT.
	num++;

	snprintf(endaddr, sizeof(endaddr), "%d", num);
	snprintf(ipv6addr, addr_len, "2001:2:0:1::%s", endaddr);

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
	char dstaddr[48] = "";
	char srcaddr[48] = "2001:2:0:0::1";
	char pkt[2048];
	struct sockaddr_in6 dst_sin;

	if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror("socket");
		return -1;
	}

	create_string_ipv6addr(dstaddr, sizeof(dstaddr), 1);
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
