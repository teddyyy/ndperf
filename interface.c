#include "ndperf.h"
#include "interface.h"

int
create_virtual_interface_name(char *ifname, int ifname_len, int num)
{
	int err;
	char ifindex[8] = "";

	err = snprintf(ifindex, sizeof(ifindex), "%d", num);
	if (err < 0) {
		perror("snprintf");
		return err;
	}

	snprintf(ifname, ifname_len, "vif%s", ifindex);
	if (err < 0) {
		perror("snprintf");
		return err;
	}

	return 0;
}

static
int up_interface(char *ifname)
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
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

static
int set_address(char *ifname, struct in6_addr *addr, int prefixlen)
{
	int fd;
	struct in6_ifreq ifr;
	int err;

	fd = socket(AF_INET6, SOCK_DGRAM, 0);

	memset(&ifr, 0, sizeof(ifr));

	memcpy(&ifr.ifr6_addr, addr, sizeof(struct in6_addr));
	ifr.ifr6_prefixlen = prefixlen;
	ifr.ifr6_ifindex = if_nametoindex(ifname);

	err = ioctl(fd, SIOCSIFADDR, &ifr);
	if (err < 0) {
		perror("ioctl");
		return err;
	}

	close(fd);

	return 0;
}

int
create_virtual_interface(struct in6_addr *addr, int prefixlen,
			 int ifnum, char *ifname)
{
	struct rtnl_link *link;
	struct nl_cache *link_cache;
	struct nl_sock *sock;
	int err, master_index;
	char vif[16] = "";
	struct in6_addr setaddr;

	memcpy(&setaddr, addr, sizeof(struct in6_addr));

	sock = nl_socket_alloc();
	if ((err = nl_connect(sock, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		return err;
	}

	if ((err = rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache)) < 0) {
		nl_perror(err, "Unable to allocate cache");
		return err;
	}

	if (!(master_index = rtnl_link_name2i(link_cache, ifname))) {
		fprintf(stderr, "Unable to lookup interface");
		return -1;
	}

	for (int i = 1; i <= ifnum; i++) {
		link = rtnl_link_macvlan_alloc();
		rtnl_link_set_link(link, master_index);

		rtnl_link_macvlan_set_mode(link,
			rtnl_link_macvlan_str2mode("private"));

		// construct interface name
		if ((err = create_virtual_interface_name(vif, sizeof(vif), i)) < 0) {
			fprintf(stderr, "Unable to create interface name\n");
			return err;
		}

		rtnl_link_set_name(link, vif);
		if ((err = rtnl_link_add(sock, link, NLM_F_CREATE)) < 0) {
			nl_perror(err, "Unable to add link");
			return err;
		}

		increment_ipv6addr_plus_one(&setaddr);
		if ((err = set_address(vif, &setaddr, prefixlen)) < 0) {
			fprintf(stderr, "Unable to set address\n");
			return err;
		}

		if ((err = up_interface(rtnl_link_get_name(link))) < 0) {
			fprintf(stderr, "Unable to up interface\n");
			return err;
		}

		rtnl_link_put(link);
	}

	nl_close(sock);

	return 0;
}

int
delete_virtual_interface(int ifnum)
{
	struct rtnl_link *link;
	struct nl_sock *sock;
	int err;
	char vif[16] = "";

	sock = nl_socket_alloc();
	if ((err = nl_connect(sock, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		return err;
	}

	for (int i = 1; i <= ifnum; i++) {
		link = rtnl_link_alloc();

		// construct interface name
                if ((err = create_virtual_interface_name(vif, sizeof(vif), i)) < 0) {
			fprintf(stderr, "Unable to create interface name\n");
			return err;
		}

		rtnl_link_set_name(link, vif);

		if ((err = rtnl_link_delete(sock, link)) < 0) {
			nl_perror(err, "Unable to delete link");
			return err;
		}

		rtnl_link_put(link);
	}

	nl_close(sock);

	return 0;
}

