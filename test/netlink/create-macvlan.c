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
int set_ip6addr(char *name, struct in6_addr *addr, int prefixlen)
{
	int fd;
	struct in6_ifreq ifr;
	int ret;

	fd = socket(AF_INET6, SOCK_DGRAM, 0);

	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr6_addr = *addr;
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

static void
create_virtual_interface_name(char *ifname, int ifname_len, int num)
{
	char ifindex[4] = "";

	snprintf(ifindex, sizeof(ifindex), "%d", num);
	snprintf(ifname, ifname_len, "vif%s", ifindex);

	return;
}

static void
increment_string_ipv6addr(struct in6_addr *addr)
{
	int i;

        // increase address by one
        addr->s6_addr[15]++;
        i = 15;
        while (i > 0 && !addr->s6_addr[i--]) addr->s6_addr[i]++;

        return;
}

int
main(int argc, char *argv[])
{
	struct rtnl_link *link;
	struct nl_cache *link_cache;
	struct nl_sock *sock;
	int i, ifnum, err, master_index;
	char ifname[8] = "";
	char addr_str[46] = "2001:2:0:1::1";
	struct in6_addr addr;

	if (argc != 2) {
                fprintf(stderr, "./create-macvlan <interface num>\n");
                return -1;
        }

        ifnum = atoi(argv[1]);

	inet_pton(AF_INET6, addr_str, &addr);

	sock = nl_socket_alloc();
	if ((err = nl_connect(sock, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		return err;
	}

	if ((err = rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache)) < 0) {
		nl_perror(err, "Unable to allocate cache");
		return err;
	}

	if (!(master_index = rtnl_link_name2i(link_cache, "enp0s3"))) {
		fprintf(stderr, "Unable to lookup enp0s3");
		return -1;
	}

	for (i = 1; i <= ifnum; i++) {
		link = rtnl_link_macvlan_alloc();
		rtnl_link_set_link(link, master_index);

		rtnl_link_macvlan_set_mode(link,
				   rtnl_link_macvlan_str2mode("private"));

		// construct interface name
		create_virtual_interface_name(ifname, sizeof(ifname), i);

		rtnl_link_set_name(link, ifname);
		if ((err = rtnl_link_add(sock, link, NLM_F_CREATE)) < 0) {
			nl_perror(err, "Unable to add link");
			return err;
		}

		// increment address
		increment_string_ipv6addr(&addr);

		set_ip6addr(ifname, &addr, 64);

		ifup(rtnl_link_get_name(link));

		rtnl_link_put(link);

	}

	nl_close(sock);

	return 0;
}
