#include <netlink/netlink.h>
#include <netlink/route/link.h>

static
char* itoa(int num)
{
        int count = 1;
        int n = num;

        while(n > 10) {n = n / 10; count++;}

        char *str = malloc(count + 1);

        str[count] = '\0';

        n = num;

        for(int i = count-1; i >= 0; i--) {
                str[i] =  (n % 10) + '0';
                n = n / 10;
        }

        return str;
}

int
main(int argc, char *argv[])
{
	struct rtnl_link *link;
	struct nl_sock *sock;
	int i, err;
	char ifname[16] = "";

	sock = nl_socket_alloc();
	if ((err = nl_connect(sock, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		return err;
	}

	for (i = 0; i < 3; i++) {
		link = rtnl_link_alloc();

		sprintf(ifname, "vif%s", itoa(i));
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
