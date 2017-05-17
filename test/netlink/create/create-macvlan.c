#include <netinet/ether.h>

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/macvlan.h>

#include <linux/netlink.h>

int
main(int argc, char *argv[])
{
	struct rtnl_link *link;
	struct nl_cache *link_cache;
	struct nl_sock *sock;
	int err, master_index;

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

	link = rtnl_link_macvlan_alloc();
	rtnl_link_set_link(link, master_index);

	rtnl_link_macvlan_set_mode(link, rtnl_link_macvlan_str2mode("private"));

	if ((err = rtnl_link_add(sock, link, NLM_F_CREATE)) < 0) {
		nl_perror(err, "Unable to add link");
		return err;
	}

	rtnl_link_put(link);

	nl_close(sock);

	return 0;
}
