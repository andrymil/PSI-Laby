#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8888
#define FILE_SIZE 10000
#define CHUNK_SIZE 100
#define NUM_CHUNKS (FILE_SIZE / CHUNK_SIZE)
#define PACKET_SIZE (CHUNK_SIZE + 4) // 4b (seq) + 100b (data)
#define MAX_BUFFER 2048 // Bufor na pakiety przychodzące

// Numery specjalne protokołu
#define PACKET_FIN -1
#define PACKET_DONE -2

int main() {
    int sockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    char buffer[MAX_BUFFER];

    // Bufor na cały plik
    char *file_buffer = (char *)malloc(FILE_SIZE);
    if (file_buffer == NULL) {
        perror("malloc file_buffer failed");
        return 1;
    }
    // Tablica flag śledząca otrzymane pakiety
    int received_chunks[NUM_CHUNKS] = {0};
    int total_received = 0;

    // --- 1. Konfiguracja gniazda UDP ---
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Umożliwia ponowne użycie adresu
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // Nasłuchujemy na WSZYSTKICH interfejsach (kluczowe dla Dockera)
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // --- 2. Bindowanie gniazda ---
    if (bind(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on 0.0.0.0:%d (UDP)\n", PORT);

    // --- 3. Główna pętla serwera ---
    while (1) {
        int n = recvfrom(sockfd, buffer, MAX_BUFFER, 0, (struct sockaddr *)&cli_addr, &cli_len);
        if (n < 4) { // Zbyt mały pakiet
            continue;
        }

        // Dekodujemy numer sekwencyjny (pierwsze 4 bajty)
        int32_t seq_num_net;
        memcpy(&seq_num_net, buffer, 4);
        int32_t seq_num = ntohl(seq_num_net);

        if (seq_num >= 0 && seq_num < NUM_CHUNKS) {
            // To jest pakiet danych
            if (n == PACKET_SIZE) {
                if (received_chunks[seq_num] == 0) {
                    // Kopiujemy dane (pomijając 4b nagłówka) do bufora pliku
                    memcpy(file_buffer + (seq_num * CHUNK_SIZE), buffer + 4, CHUNK_SIZE);
                    received_chunks[seq_num] = 1;
                    total_received++;
                    printf("Received packet %d. Total received: %d/%d\n", seq_num, total_received, NUM_CHUNKS);
                }
            }
        } else if (seq_num == PACKET_FIN) {
            // Klient pyta o status
            printf("FIN (-1) received. Checking status...\n");

            int missing_count = 0;
            int i;
            for (i = 0; i < NUM_CHUNKS; i++) {
                if (received_chunks[i] == 0) {
                    missing_count++;
                }
            }

            if (missing_count == 0) {
                // Mamy wszystko, wyślij DONE i zakończ
                printf("All packets received. Sending DONE (-2).\n");
                int32_t done_pkt = htonl(PACKET_DONE);
                sendto(sockfd, &done_pkt, 4, 0, (struct sockaddr *)&cli_addr, cli_len);
                // Wyślij kilka razy na wypadek, gdyby DONE się zgubił
                sendto(sockfd, &done_pkt, 4, 0, (struct sockaddr *)&cli_addr, cli_len);
                break; // Wyjście z pętli while(1)
            } else {
                // Przygotuj pakiet NAK (Negative Acknowledgment)
                printf("%d packets missing. Sending NAK list.\n", missing_count);
                int nak_pkt_size = 4 + (missing_count * 4);
                char *nak_pkt = (char *)malloc(nak_pkt_size);

                *(int32_t*)nak_pkt = htonl(missing_count); // Pierwsze 4b = ilość

                int j = 1;
                for (i = 0; i < NUM_CHUNKS; i++) {
                    if (received_chunks[i] == 0) {
                        *(int32_t*)(nak_pkt + (j * 4)) = htonl(i);
                        j++;
                    }
                }
                sendto(sockfd, nak_pkt, nak_pkt_size, 0, (struct sockaddr *)&cli_addr, cli_len);
                free(nak_pkt);
            }
        }
    }

    // --- 4. Zapisz plik i oblicz hash ---
    printf("\nReconstruction complete.\n");

    // Zapisz zrekonstruowany plik na dysku
    FILE *fp = fopen("reconstructed.dat", "wb");
    if (fp == NULL) {
        perror("fopen failed");
    } else {
        fwrite(file_buffer, 1, FILE_SIZE, fp);
        fclose(fp);
        printf("File saved as reconstructed.dat.\n");

        // Oblicz hash SHA-256 za pomocą polecenia systemowego
        printf("Calculating SHA-256 hash...\n");
        char command[100];
        sprintf(command, "sha256sum reconstructed.dat");

        printf("--- VERIFICATION --- (Compare with client's hash)\n");
        fflush(stdout); // Wymuś opróżnienie bufora przed wywołaniem system()
        system(command);
        fflush(stdout);
    }

    free(file_buffer);
    close(sockfd);
    printf("Server shutting down.\n");
    return 0;
}