#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define HOST "server"
#define PORT 8888

#define FILE_SIZE 10000
#define CHUNK_SIZE 100
#define NUM_CHUNKS ((FILE_SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE)
#define PACKET_SIZE (CHUNK_SIZE + 4)

#define FIN_PACKET -1
#define CLIENT_TIMEOUT 1

char *prepare_packets(unsigned char *hash_out) {
  unsigned char *file_data = (unsigned char *)malloc(FILE_SIZE);
  if (file_data == NULL) {
    perror("malloc file_data failed");
    return NULL;
  }

  FILE *urandom = fopen("/dev/urandom", "r");
  if (urandom == NULL) {
    perror("fopen /dev/urandom failed");
    free(file_data);
    return NULL;
  }
  fread(file_data, 1, FILE_SIZE, urandom);
  fclose(urandom);

  SHA256_CTX sha_ctx;
  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx, file_data, FILE_SIZE);
  SHA256_Final(hash_out, &sha_ctx);

  char *packets_buffer = (char *)malloc(NUM_CHUNKS * PACKET_SIZE);
  if (packets_buffer == NULL) {
    perror("malloc packets_buffer failed");
    free(file_data);
    return NULL;
  }

  for (int i = 0; i < NUM_CHUNKS; i++) {
    char *packet_ptr = packets_buffer + (i * PACKET_SIZE);
    int32_t sequence_num = htonl(i);
    memcpy(packet_ptr, &sequence_num, 4);
    memcpy(packet_ptr + 4, file_data + (i * CHUNK_SIZE), CHUNK_SIZE);
  }

  free(file_data);
  return packets_buffer;
}

int setup_socket() {
  int sockfd;
  struct hostent *server_host;
  struct sockaddr_in server_addr;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    return -1;
  }

  struct timeval tv;
  tv.tv_sec = CLIENT_TIMEOUT;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    perror("setsockopt SO_RCVTIMEO failed");
    close(sockfd);
    return -1;
  }

  server_host = gethostbyname(HOST);
  if (server_host == NULL) {
    fprintf(stderr, "ERROR, no such host as %s\n", HOST);
    close(sockfd);
    return -1;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  memcpy(&server_addr.sin_addr, server_host->h_addr_list[0],
         server_host->h_length);

  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect failed");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

void run_transfer(int sockfd, char *packets_buffer) {
  printf("Sending all %d packets...\n", NUM_CHUNKS);
  for (int i = 0; i < NUM_CHUNKS; i++) {
    char *packet_ptr = packets_buffer + (i * PACKET_SIZE);
    send(sockfd, packet_ptr, PACKET_SIZE, 0);
  }
  printf("Packets sent.\n");

  int32_t fin_packet = htonl(FIN_PACKET);
  char recv_buffer[4096];

  while (1) {
    printf("Sending FIN packet (-1)...\n");
    send(sockfd, &fin_packet, sizeof(fin_packet), 0);

    int n = recv(sockfd, recv_buffer, 4096, 0);

    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        printf("Timeout (%ds) waiting for server ACK/NAK.\n", CLIENT_TIMEOUT);
        continue;
      } else {
        perror("recvfrom error");
        break;
      }
    }

    if (n < 4)
      continue;

    int32_t resp_type = ntohl(*(int32_t *)recv_buffer);

    if (resp_type == 0) {
      printf("\nServer received all packets. File transfer complete.\n");
      break;
    } else if (resp_type > 0) {
      int num_missing = resp_type;
      printf("Server sent NAK. Missing %d packets.\n", num_missing);

      if (n < 4 + (num_missing * 4)) {
        printf("Received incomplete NAK packet!\n");
        continue;
      }

      for (int i = 0; i < num_missing; i++) {
        int32_t missing_seq = ntohl(*(int32_t *)(recv_buffer + 4 + (i * 4)));

        if (missing_seq < 0 || missing_seq >= NUM_CHUNKS) {
          printf("Server requested non-existent packet %d\n", missing_seq);
          continue;
        }

        char *packet_ptr = packets_buffer + (missing_seq * PACKET_SIZE);
        send(sockfd, packet_ptr, PACKET_SIZE, 0);
      }
    }
  }
}

void print_hash(unsigned char hash[SHA256_DIGEST_LENGTH]) {
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    printf("%02x", hash[i]);
  }
  printf("\n");
}

int main() {
  unsigned char file_hash[SHA256_DIGEST_LENGTH];
  char *packets_buffer = NULL;
  int sockfd = -1;

  packets_buffer = prepare_packets(file_hash);
  if (packets_buffer == NULL) {
    fprintf(stderr, "Error preparing packets.\n");
    return 1;
  }

  sockfd = setup_socket();
  if (sockfd < 0) {
    fprintf(stderr, "Error setting up the socket.\n");
    free(packets_buffer);
    return 1;
  }

  run_transfer(sockfd, packets_buffer);

  close(sockfd);
  free(packets_buffer);

  printf("--- VERIFICATION ---\n");
  printf("Client hash: ");
  print_hash(file_hash);
  printf("Client shutting down.\n");

  return 0;
}
