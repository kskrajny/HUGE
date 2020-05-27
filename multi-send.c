#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <errno.h>

#include "err.h"

#define BUF_SIZE      128
#define TTL_VALUE     4

struct protocol {
  short unsigned int type;
  short unsigned int len;
  char buf[BUF_SIZE];
};

int main(int argc, char *argv[]) {
    /* argumenty wywołania programu */
    char *remote_dotted_address;
    in_port_t remote_port;

    /* zmienne i struktury opisujące gniazda */
    int sock, optval;
    struct sockaddr_in remote_address, local_address;
    socklen_t rcva_len;

    /* zmienne do poll */
    int ret;
    char peeraddr[BUF_SIZE];

    /* parsowanie argumentów programu */
    if (argc != 3)
        fatal("Usage: %s multicast_dotted_address serer_port\n", argv[0]);
    remote_dotted_address = argv[1];
    remote_port = (in_port_t) atoi(argv[2]);

    /* otwarcie gniazda */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    /* uaktywnienie rozgłaszania (ang. broadcast) */
    optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &optval, sizeof optval) < 0)
        syserr("setsockopt broadcast");

    /* ustawienie TTL dla datagramów rozsyłanych do grupy */
    optval = TTL_VALUE;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optval, sizeof optval) < 0)
        syserr("setsockopt multicast ttl");

    /* ustawienie adresu i portu odbiorcy */
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(remote_port);
    if (inet_aton(remote_dotted_address, &remote_address.sin_addr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }

    /* ustawienie adresu i portu lokalnego */
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = 20000;
    if (bind(sock, (struct sockaddr *) &local_address, sizeof local_address) < 0)
        syserr("bind");

    /* ustawienia do poll */
    struct pollfd fds[2];
    fds[0].fd = sock;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = 0;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    struct protocol pro;
    pro.len = 0;
    pro.type = 1;

    /* proszenie o podanie czasu */
    while(1) {
        ret = poll((struct pollfd *)&fds, 2, 3000);
        if (ret == -1) {
            if (errno == EINTR)
                fprintf(stderr, "Interrupted system call\n");
            else
                syserr("poll");
        } else if (ret > 0) {
          if(fds[0].revents & POLLIN) {
            rcva_len = (socklen_t) sizeof(local_address);
            recvfrom(sock, &pro, sizeof(struct protocol), 0, (struct sockaddr *) &local_address, &rcva_len);
            /* zmiana na hostowy porządek bajtów */
            pro.len = ntohs(pro.len);
            pro.type = ntohs(pro.type);
            if(inet_ntop(AF_INET, &local_address.sin_addr, peeraddr, BUF_SIZE) == NULL)
                syserr("inet_ntop");
            if(pro.type == 2)
                printf("Radio: %s\n", pro.buf);
            if(pro.type == 6)
                printf("METALEN: %d\n", pro.len);
          }
          if(fds[1].revents & POLLIN) {
            memset(pro.buf, 0, BUF_SIZE);
            scanf("%hd", &pro.type);
            /* zmiana na sieciowy porządek bajtów */
            pro.len = htons(pro.len);
            pro.type = htons(pro.type); 
            if(sendto(sock, &pro, sizeof(struct protocol), 0, (struct sockaddr *) &remote_address, sizeof(remote_address)) < 0)
              syserr("write");
          }
        }
    }
    /* koniec */
    close(sock);
}