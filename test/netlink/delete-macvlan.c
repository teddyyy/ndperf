#include <netlink/netlink.h>
#include <netlink/route/link.h>

static void
create_virtual_interface_name(char *ifname, int ifname_len, int num)
{
        char ifindex[4] = "";

        snprintf(ifindex, sizeof(ifindex), "%d", num);
        snprintf(ifname, ifname_len, "vif%s", ifindex);

        return;
}

int
main(int argc, char *argv[])
{
	struct rtnl_link *link;
	struct nl_sock *sock;
	int i, ifnum, err;
	char ifname[8] = "";

	if (argc != 2) {
		fprintf(stderr, "./delete-macvlan <interface num>\n");
		return -1;
	}

	ifnum = atoi(argv[1]);

	sock = nl_socket_alloc();
	if ((err = nl_connect(sock, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		return err;
	}

	for (i = 1; i <= ifnum; i++) {
		link = rtnl_link_alloc();

		// construct interface name
                create_virtual_interface_name(ifname, sizeof(ifname), i);

		rtnl_link_set_name(link, ifname);

		if ((err = rtnl_link_delete(sock, link)) < 0) {
			nl_perror(err, "Unable to delete link");
			return err;
		}

		rtnl_link_put(link);
	}

	nl_close(sock);

	return 0;
}
