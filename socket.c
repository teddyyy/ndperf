#include "ndperf.h"
#include "socket.h"

int
init_tx_socket(char *ifname)
{
        int sock;

        if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
                perror("socket");
                return -1;
        }

        // bind specific interface
        setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifname, strlen(ifname) + 1);

        return sock;
}

int
init_rx_socket(int ifnum)
{
	int sock, err;
	char vif[16] = "";

	if ((sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IPV6))) < 0) {
                perror("socket");
                return -1;
        }

	for (int i = 1; i <= ifnum; i++) {
		// construct interface name
                if ((err = create_virtual_interface_name(vif, sizeof(vif), i)) < 0) {
			fprintf(stderr, "Unable to create interface name\n");
			return err;
		}

		// bind specific interface
		setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, vif, strlen(vif) + 1);
	}

	return sock;
}

