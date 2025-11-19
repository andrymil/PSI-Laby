#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int port = (argc > 1) ? atoi(argv[1]) : 8888;

  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return 1;
  }

  printf("UDP server listening on 0.0.0.0:%d\n", port);
  fflush(stdout);

  char buf[65536];
  char ack = 'A';
  struct sockaddr_in cli;
  socklen_t clen = sizeof(cli);

  for (;;) {
    ssize_t n =
        recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&cli, &clen);
    if (n < 0) {
      perror("recvfrom");
      continue;
    }

    printf("rx=%zd B from %s:%d -> ACK\n", n, inet_ntoa(cli.sin_addr),
           ntohs(cli.sin_port));
    fflush(stdout);

    if (sendto(s, &ack, 1, 0, (struct sockaddr *)&cli, clen) < 0)
      perror("sendto");
  }
}
