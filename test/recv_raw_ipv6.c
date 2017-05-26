#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int
main(int argc, char *argv[])
{
	int sock;

	if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) {
                perror("socket");
                return -1;
        }

	setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
		   "vif1", strlen("vif1") + 1);
	setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
		   "vif2", strlen("vif2") + 1);
	setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
		   "vif3", strlen("vif3") + 1);

	return 0;
}
