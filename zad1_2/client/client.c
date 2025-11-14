#include <arpa/inet.h>
#include <errno.h>       // Dla sprawdzania timeout (EAGAIN, EWOULDBLOCK)
#include <netdb.h>       // Dla gethostbyname()
#include <openssl/sha.h> // Dla SHA256
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // Dla setsockopt() timeout
#include <unistd.h>   // Dla close(), sleep()

// Używamy nazwy usługi 'server' z docker-compose.yml
// Docker sam znajdzie jej IP.
#define HOST "server"
#define PORT 8888

#define FILE_SIZE 10000
#define CHUNK_SIZE 100
#define NUM_CHUNKS (FILE_SIZE / CHUNK_SIZE)
#define PACKET_DATA_SIZE (CHUNK_SIZE + 4) // 4b (seq) + 100b (data)

// Numery specjalne protokołu
#define PACKET_FIN -1
#define PACKET_DONE -2
#define CLIENT_TIMEOUT_SEC 2 // sekundy

// Funkcja pomocnicza do drukowania hasha SHA-256 w hex
void print_hash(unsigned char hash[SHA256_DIGEST_LENGTH]) {
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    printf("%02x", hash[i]);
  }
  printf("\n");
}

int main() {
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server_host;
  socklen_t serv_len = sizeof(serv_addr);

  // --- 1. Wygeneruj plik i oblicz hash ---
  printf("Generating random file (%d bytes)\n", FILE_SIZE);
  unsigned char *file_data = (unsigned char *)malloc(FILE_SIZE);
  if (file_data == NULL) {
    perror("malloc file_data failed");
    return 1;
  }

  // Użyj /dev/urandom do wygenerowania danych
  FILE *urandom = fopen("/dev/urandom", "r");
  if (urandom == NULL) {
    perror("fopen /dev/urandom failed");
    free(file_data);
    return 1;
  }
  fread(file_data, 1, FILE_SIZE, urandom);
  fclose(urandom);

  // Oblicz lokalny hash
  unsigned char local_hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha_ctx;
  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx, file_data, FILE_SIZE);
  SHA256_Final(local_hash, &sha_ctx);

  printf("Local file hash: ");
  print_hash(local_hash);
  printf("\n");

  // --- 2. Przygotuj wszystkie pakiety w pamięci ---
  // (Łatwiejsza retransmisja)
  char *chunks_buffer = (char *)malloc(NUM_CHUNKS * PACKET_DATA_SIZE);
  if (chunks_buffer == NULL) {
    perror("malloc chunks_buffer failed");
    free(file_data);
    return 1;
  }

  for (int i = 0; i < NUM_CHUNKS; i++) {
    char *packet_ptr = chunks_buffer + (i * PACKET_DATA_SIZE);

    // Zapakuj numer sekwencyjny (network byte order)
    int32_t seq_net = htonl(i);
    memcpy(packet_ptr, &seq_net, 4);

    // Skopiuj dane
    memcpy(packet_ptr + 4, file_data + (i * CHUNK_SIZE), CHUNK_SIZE);
  }
  free(file_data); // Już nie potrzebujemy oryginalnych danych

  // --- 3. Skonfiguruj gniazdo UDP ---
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    free(chunks_buffer);
    return 1;
  }

  // Ustaw timeout na odbieranie (recvfrom)
  struct timeval tv;
  tv.tv_sec = CLIENT_TIMEOUT_SEC;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
                 sizeof(tv)) < 0) {
    perror("setsockopt SO_RCVTIMEO failed");
    close(sockfd);
    free(chunks_buffer);
    return 1;
  }

  // Znajdź IP serwera (Docker DNS)
  server_host = gethostbyname(HOST);
  if (server_host == NULL) {
    fprintf(stderr, "ERROR, no such host as %s\n", HOST);
    close(sockfd);
    free(chunks_buffer);
    return 1;
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  memcpy(&serv_addr.sin_addr, server_host->h_addr_list[0],
         server_host->h_length);

  // --- 4. Wyślij pierwszą partię wszystkich pakietów ---
  printf("Sending all %d packets (initial burst)...\n", NUM_CHUNKS);
  for (int i = 0; i < NUM_CHUNKS; i++) {
    char *packet_ptr = chunks_buffer + (i * PACKET_DATA_SIZE);
    sendto(sockfd, packet_ptr, PACKET_DATA_SIZE, 0,
           (struct sockaddr *)&serv_addr, serv_len);
  }
  printf("Initial burst sent.\n");

  // --- 5. Pętla retransmisji (NAK) ---
  int32_t fin_pkt = htonl(PACKET_FIN);
  char recv_buffer[4096]; // Bufor na listę NAK

  while (1) {
    printf("Sending FIN packet (-1)...\n");
    sendto(sockfd, &fin_pkt, sizeof(fin_pkt), 0, (struct sockaddr *)&serv_addr,
           serv_len);

    // Czekaj na odpowiedź (NAK lub DONE)
    int n = recvfrom(sockfd, recv_buffer, 4096, 0, NULL, NULL);

    if (n < 0) {
      // Sprawdź czy to timeout
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        printf("Timeout (%ds) waiting for server ACK/NAK.\n",
               CLIENT_TIMEOUT_SEC);
        // Nasz FIN lub odpowiedź serwera (NAK/DONE) zostały zgubione.
        // Pętla po prostu powtórzy wysłanie FIN.
        continue;
      } else {
        perror("recvfrom error");
        break;
      }
    }

    if (n < 4) { // Za mały pakiet
      continue;
    }

    // Rozpakuj typ odpowiedzi
    int32_t resp_type = ntohl(*(int32_t *)recv_buffer);

    if (resp_type == PACKET_DONE) {
      printf("\nServer sent DONE (-2). File transfer complete.\n");
      break; // Sukces, wyjdź z pętli while(1)

    } else if (resp_type == 0) {
      printf("\nServer sent NAK with 0 missing. File transfer complete.\n");
      break; // Traktujemy to jak sukces

    } else if (resp_type > 0) {
      // To jest NAK, resp_type to liczba brakujących pakietów
      int num_missing = resp_type;
      printf("Server sent NAK. Missing %d packets.\n", num_missing);

      // Sprawdź, czy pakiet NAK jest kompletny
      if (n < 4 + (num_missing * 4)) {
        printf("Received incomplete NAK packet!\n");
        continue;
      }

      // Retransmituj brakujące pakiety
      for (int i = 0; i < num_missing; i++) {
        int32_t missing_seq = ntohl(*(int32_t *)(recv_buffer + 4 + (i * 4)));

        if (missing_seq >= 0 && missing_seq < NUM_CHUNKS) {
          // printf("Retransmitting packet %d\n", missing_seq);
          char *packet_ptr = chunks_buffer + (missing_seq * PACKET_DATA_SIZE);
          sendto(sockfd, packet_ptr, PACKET_DATA_SIZE, 0,
                 (struct sockaddr *)&serv_addr, serv_len);
        } else {
          printf("Error: Server requested non-existent packet %d\n",
                 missing_seq);
        }
      }
    }
  }

  // --- 6. Sprzątanie ---
  close(sockfd);
  free(chunks_buffer);

  printf("Client shutting down.\n");
  printf("--- VERIFICATION ---\n");
  printf("Client local hash: ");
  print_hash(local_hash);
  printf("(Compare this with the hash printed by the server console)\n");

  return 0;
}