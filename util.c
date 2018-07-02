#include "ndperf.h"
#include "util.h"

void
increment_ipv6addr_plus_one(struct in6_addr *addr)
{
        int i;

        // increase address by one
        addr->s6_addr[15]++;
        i = 15;
        while (i > 0 && !addr->s6_addr[i--]) addr->s6_addr[i]++;
}

int
get_tx_link_speed(char *ifname)
{
        int sock, ret;
        struct ifreq ifr;
        struct ethtool_cmd e;

        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
                perror("socket");
                return -1;
        }

        strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
        ifr.ifr_data = (caddr_t)&e;

        e.cmd = ETHTOOL_GSET;

        ret = ioctl(sock, SIOCETHTOOL, &ifr);
        if (ret < 0) {
                perror("ioctl");
                return ret;
        }

        switch (ethtool_cmd_speed(&e)) {
                case SPEED_10: ret = 10; break;
                case SPEED_100: ret = 100; break;
                case SPEED_1000: ret = 1000; break;
                case SPEED_2500: ret = 2500; break;
                case SPEED_10000: ret = 10000; break;
                default: fprintf(stderr, "Link Speed returned is %d\n", e.speed); ret = -1;
        }

        close(sock);

        return ret;
}

long get_packet_count(char *filepath)
{
        int fd;
        char buf[48];

        fd = open(filepath, O_RDONLY);
        if (fd == -1) {
                perror("open");
                return -1;
        }

        if (read(fd, buf, sizeof(buf)) == -1) {
                perror("read");
                return -1;
        }

        close(fd);

        return atol(buf);
}

