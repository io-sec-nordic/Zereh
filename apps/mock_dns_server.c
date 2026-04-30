#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int fd;
    struct sockaddr_in addr;
    uint8_t buf[2048];
    uint16_t port = 5302;

    if (argc > 1) {
        port = (uint16_t)strtoul(argv[1], NULL, 10);
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(fd);
        return 1;
    }

    printf("mock_dns_server listening on UDP %u\n", port);

    while (1) {
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);
        ssize_t n;
        char ip[INET_ADDRSTRLEN] = {0};

        n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&src, &src_len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recvfrom");
            break;
        }

        inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));
        printf("mock_dns_server packet: bytes=%zd from=%s:%u\n", n, ip, ntohs(src.sin_port));
    }

    close(fd);
    return 0;
}
