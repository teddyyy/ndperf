#include <unistd.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/macvlan.h>

#include <linux/netlink.h>

struct in6_ifreq
{
	struct in6_addr ifr6_addr;
	u_int32_t ifr6_prefixlen;
	int ifr6_ifindex;
};

static
char* itoa(int num)
{
	int count = 1;
	int n = num;

	while (n > 10) {n = n / 10; count++;}

	char *str = malloc(count + 1);

	str[count] = '\0';

	n = num;

	for (int i = count-1; i >= 0; i--) {
		str[i] =  (n % 10) + '0';
		n = n / 10;
	}

	return str;
}

static
int set_ip6addr(char *name, char *ip6addr, int prefixlen)
{
	int fd;
	struct in6_ifreq ifr;
	int ret;

	fd = socket(AF_INET6, SOCK_DGRAM, 0);

	memset(&ifr, 0, sizeof(ifr));
	inet_pton(AF_INET6, ip6addr, &ifr.ifr6_addr);

	ifr.ifr6_prefixlen = prefixlen;
	ifr.ifr6_ifindex = if_nametoindex(name);

	ret = ioctl(fd, SIOCSIFADDR, &ifr);
	if (ret < 0) {
		perror("ioctl");
		return -1;
	}

	close(fd);

	return 0;
}


static
int ifup(char *name)
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	strncpy(ifr.ifr_name, name, IFNAMSIZ-1);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) != 0) {
		perror("ioctl");
		return -1;
	}

	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

	if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0) {
		perror("ioctl");
		return -1;
	}

	close(fd);

	return 0;
}

int
main(int argc, char *argv[])
{
	struct rtnl_link *link;
	struct nl_cache *link_cache;
	struct nl_sock *sock;
	int i, n, err, master_index;
	char ifname[16] = "";
	char addr[48] = "";

	sock = nl_socket_alloc();
	if ((err = nl_connect(sock, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		return err;
	}

	if ((err = rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache)) < 0) {
		nl_perror(err, "Unable to allocate cache");
		return err;
	}

	if (!(master_index = rtnl_link_name2i(link_cache, "enp0s8"))) {
		fprintf(stderr, "Unable to lookup enp0s3");
		return -1;
	}

	for (i = 0; i < 3; i++) {
		link = rtnl_link_macvlan_alloc();
		rtnl_link_set_link(link, master_index);

		rtnl_link_macvlan_set_mode(link,
				   rtnl_link_macvlan_str2mode("private"));

		// construct interface name
		sprintf(ifname, "vif%s", itoa(i));
		rtnl_link_set_name(link, ifname);
		if ((err = rtnl_link_add(sock, link, NLM_F_CREATE)) < 0) {
			nl_perror(err, "Unable to add link");
			return err;
		}

		/* Since it is used with the getway and the DUT,
		   0 and 1 are not used */
		n = i + 2;
		sprintf(addr, "2001:2:0:1::%s", itoa(n));
		set_ip6addr(ifname, addr, 64);

		ifup(rtnl_link_get_name(link));

		rtnl_link_put(link);
	}

	nl_close(sock);

	return 0;
}
