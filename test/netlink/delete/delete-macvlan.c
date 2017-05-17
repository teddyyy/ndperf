#include <netlink/netlink.h>
#include <netlink/route/link.h>

int
main(int argc, char *argv[])
{
	struct rtnl_link *link;
	struct nl_sock *sock;
	int err;

	sock = nl_socket_alloc();
	if ((err = nl_connect(sock, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		return err;
	}

	link = rtnl_link_alloc();
	rtnl_link_set_name(link, "macvlan0");

	if ((err = rtnl_link_delete(sock, link)) < 0) {
		nl_perror(err, "Unable to delete link");
		return err;
	}

	rtnl_link_put(link);
	nl_close(sock);

	return 0;
}
