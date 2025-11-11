#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 7000000

#define bailout(s) { perror( s ); exit(1);  }
#define Usage() { errx( 0, "Usage: %s address-or-ip [port]\n", argv[0]); }

int main(int argc, char *argv[]) {
    int                      sfd, s;
    char                     buf[BUF_SIZE];
    char *response = "ACK";
    ssize_t                  nread;
    socklen_t                peer_addrlen;
    struct sockaddr_in       server, client;
    struct sockaddr_storage  peer_addr;

    if (argc != 2)
        Usage();

    if ( (sfd=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	    bailout("socker() ");

   server.sin_family      = AF_INET;  /* Server is in Internet Domain */
   server.sin_port        = htons(atoi(argv[1]));         /* Use any available port      */
   server.sin_addr.s_addr = INADDR_ANY; /* Server's Internet Address   */

   if ( (s=bind(sfd, (struct sockaddr *)&server, sizeof(server))) < 0)
      bailout("bind() ");
   printf("bind() successful\n");


    /* Read datagrams and echo them back to sender. */
   printf("waiting for packets...\n");

    for (;;) {
        peer_addrlen = sizeof(peer_addr);
        nread = recvfrom(sfd, buf, BUF_SIZE, 0,
                         (struct sockaddr *) &peer_addr, &peer_addrlen);
        printf("recvfrom ok, packets: %zd\n", nread);
            if (nread <0 ) {
            fprintf(stderr, "failed recvfrom\n");
                continue;               /* Ignore failed request */
            }
            else if (nread == 0) {
                printf("End of file on socket\n");
                break;
            }

        sendto(sfd, (const char *)response, strlen(response), 0, (struct sockaddr *) &peer_addr, peer_addrlen);
        printf("sendto ok\n");
    }
}
