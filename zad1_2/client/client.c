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
#define NUM_CHUNKS (FILE_SIZE / CHUNK_SIZE)
#define PACKET_DATA_SIZE (CHUNK_SIZE + 4) // 4b (seq) + 100b (data)

// Numery specjalne protokołu
#define PACKET_FIN -1
#define PACKET_DONE -2
#define CLIENT_TIMEOUT_SEC 2 // sekundy

// --- PROTOTYPY FUNKCJI ---
void print_hash(unsigned char hash[SHA256_DIGEST_LENGTH]);
char *prepare_packets(unsigned char *hash_out);
int setup_socket(struct sockaddr_in *serv_addr_out);
void run_transfer(int sockfd, char *chunks_buffer,
                  const struct sockaddr_in *serv_addr);

// --- FUNKCJA GŁÓWNA ---
int main() {
  unsigned char local_hash[SHA256_DIGEST_LENGTH];
  char *chunks_buffer = NULL;
  int sockfd = -1;

  // 1. Przygotuj dane i pakiety
  chunks_buffer = prepare_packets(local_hash);
  if (chunks_buffer == NULL) {
    fprintf(stderr, "Błąd podczas przygotowywania pakietów.\n");
    return 1;
  }

  // 2. Skonfiguruj gniazdo
  struct sockaddr_in serv_addr;
  sockfd = setup_socket(&serv_addr);
  if (sockfd < 0) {
    fprintf(stderr, "Błąd podczas konfiguracji gniazda.\n");
    free(chunks_buffer);
    return 1;
  }

  // 3. Uruchom logikę transferu (burst + pętla NAK)
  run_transfer(sockfd, chunks_buffer, &serv_addr);

  // 4. Sprzątanie
  close(sockfd);
  free(chunks_buffer);

  printf("Client shutting down.\n");
  printf("--- VERIFICATION ---\n");
  printf("Client local hash: ");
  print_hash(local_hash);
  printf("(Compare this with the hash printed by the server console)\n");

  return 0;
}

/**
 * @brief Generuje losowe dane, oblicza ich hash i przygotowuje bufor
 * ze wszystkimi pakietami gotowymi do wysłania.
 * @param hash_out Wskaźnik do bufora (256 bitów) na wynikowy hash.
 * @return Wskaźnik na zaalokowany bufor z pakietami lub NULL w przypadku błędu.
 */
char *prepare_packets(unsigned char *hash_out) {
  // --- 1. Wygeneruj plik i oblicz hash ---
  printf("Generating random file (%d bytes)\n", FILE_SIZE);
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

  // Oblicz lokalny hash
  SHA256_CTX sha_ctx;
  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx, file_data, FILE_SIZE);
  SHA256_Final(hash_out, &sha_ctx);

  printf("Local file hash: ");
  print_hash(hash_out);
  printf("\n");

  // --- 2. Przygotuj wszystkie pakiety w pamięci ---
  char *chunks_buffer = (char *)malloc(NUM_CHUNKS * PACKET_DATA_SIZE);
  if (chunks_buffer == NULL) {
    perror("malloc chunks_buffer failed");
    free(file_data);
    return NULL;
  }

  for (int i = 0; i < NUM_CHUNKS; i++) {
    char *packet_ptr = chunks_buffer + (i * PACKET_DATA_SIZE);
    int32_t seq_net = htonl(i);
    memcpy(packet_ptr, &seq_net, 4); // Nagłówek (numer sekw.)
    memcpy(packet_ptr + 4, file_data + (i * CHUNK_SIZE), CHUNK_SIZE); // Dane
  }

  free(file_data); // Oryginalne dane pliku nie są już potrzebne
  return chunks_buffer;
}

/**
 * @brief Tworzy gniazdo UDP, ustawia timeout i znajduje adres serwera.
 * @param serv_addr_out Wskaźnik do struktury, która zostanie wypełniona
 * adresem serwera.
 * @return Deskryptor gniazda (sockfd) lub -1 w przypadku błędu.
 */
int setup_socket(struct sockaddr_in *serv_addr_out) {
  int sockfd;
  struct hostent *server_host;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    return -1;
  }

  // Ustaw timeout na odbieranie (recvfrom)
  struct timeval tv;
  tv.tv_sec = CLIENT_TIMEOUT_SEC;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
                 sizeof(tv)) < 0) {
    perror("setsockopt SO_RCVTIMEO failed");
    close(sockfd);
    return -1;
  }

  // Znajdź IP serwera (Docker DNS)
  server_host = gethostbyname(HOST);
  if (server_host == NULL) {
    fprintf(stderr, "ERROR, no such host as %s\n", HOST);
    close(sockfd);
    return -1;
  }

  // Wypełnij strukturę adresu serwera
  memset(serv_addr_out, 0, sizeof(*serv_addr_out));
  serv_addr_out->sin_family = AF_INET;
  serv_addr_out->sin_port = htons(PORT);
  memcpy(&serv_addr_out->sin_addr, server_host->h_addr_list[0],
         server_host->h_length);

  return sockfd;
}

/**
 * @brief Realizuje główną logikę transferu: wysyła pierwszą partię
 * danych, a następnie wchodzi w pętlę retransmisji FIN/NAK.
 * @param sockfd Deskryptor gniazda.
 * @param chunks_buffer Bufor ze wszystkimi pakietami.
 * @param serv_addr Adres serwera docelowego.
 */
void run_transfer(int sockfd, char *chunks_buffer,
                  const struct sockaddr_in *serv_addr) {
  socklen_t serv_len = sizeof(*serv_addr);

  // --- 4. Wyślij pierwszą partię wszystkich pakietów ---
  printf("Sending all %d packets (initial burst)...\n", NUM_CHUNKS);
  for (int i = 0; i < NUM_CHUNKS; i++) {
    char *packet_ptr = chunks_buffer + (i * PACKET_DATA_SIZE);
    sendto(sockfd, packet_ptr, PACKET_DATA_SIZE, 0,
           (struct sockaddr *)serv_addr, serv_len);
  }
  printf("Initial burst sent.\n");

  // --- 5. Pętla retransmisji (NAK) ---
  int32_t fin_pkt = htonl(PACKET_FIN);
  char recv_buffer[4096]; // Bufor na listę NAK

  while (1) {
    printf("Sending FIN packet (-1)...\n");
    sendto(sockfd, &fin_pkt, sizeof(fin_pkt), 0, (struct sockaddr *)serv_addr,
           serv_len);

    // Czekaj na odpowiedź (NAK lub DONE)
    int n = recvfrom(sockfd, recv_buffer, 4096, 0, NULL, NULL);

    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        printf("Timeout (%ds) waiting for server ACK/NAK.\n",
               CLIENT_TIMEOUT_SEC);
        continue; // Pętla powtórzy wysłanie FIN
      } else {
        perror("recvfrom error");
        break; // Poważny błąd
      }
    }

    if (n < 4)
      continue; // Zbyt mały pakiet

    int32_t resp_type = ntohl(*(int32_t *)recv_buffer);

    if (resp_type == PACKET_DONE) {
      printf("\nServer sent DONE (-2). File transfer complete.\n");
      break; // Sukces
    } else if (resp_type == 0) {
      printf("\nServer sent NAK with 0 missing. File transfer complete.\n");
      break; // Traktujemy to jak sukces
    } else if (resp_type > 0) {
      // To jest NAK (lista brakujących)
      int num_missing = resp_type;
      printf("Server sent NAK. Missing %d packets.\n", num_missing);

      if (n < 4 + (num_missing * 4)) {
        printf("Received incomplete NAK packet!\n");
        continue;
      }

      // Retransmituj brakujące pakiety
      for (int i = 0; i < num_missing; i++) {
        int32_t missing_seq = ntohl(*(int32_t *)(recv_buffer + 4 + (i * 4)));
        if (missing_seq >= 0 && missing_seq < NUM_CHUNKS) {
          char *packet_ptr = chunks_buffer + (missing_seq * PACKET_DATA_SIZE);
          sendto(sockfd, packet_ptr, PACKET_DATA_SIZE, 0,
                 (struct sockaddr *)serv_addr, serv_len);
        } else {
          printf("Error: Server requested non-existent packet %d\n",
                 missing_seq);
        }
      }
    }
  }
}

/**
 * @brief Funkcja pomocnicza do drukowania hasha SHA-256 w hex.
 */
void print_hash(unsigned char hash[SHA256_DIGEST_LENGTH]) {
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    printf("%02x", hash[i]);
  }
  printf("\n");
}