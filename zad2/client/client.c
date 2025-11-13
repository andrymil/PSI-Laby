#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // close()
#include <arpa/inet.h>  // htonl(), inet_pton()
#include <sys/socket.h> // socket(), connect()

#define HOST "server"
#define PORT 8888
#define NUM_NODES 15

// Struktura węzła drzewa w C
typedef struct Node {
    int value;
    struct Node *left;
    struct Node *right;
} Node;

// Struktura pakietu wysyłanego przez sieć
// (Musimy zapewnić, że jest spakowana i zgodna z oczekiwaniami Pythona)
// 'ii' w Pythonie to dwa inty. Zakładamy, że int ma 4 bajty.
#pragma pack(push, 1) // Dokładne pakowanie
typedef struct {
    int32_t index; // Używamy typów o stałym rozmiarze
    int32_t value; // dla pewności
} Packet;
#pragma pack(pop)

// Funkcja do tworzenia nowego węzła
Node* createNode(int value) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    newNode->value = value;
    newNode->left = NULL;
    newNode->right = NULL;
    return newNode;
}

// Funkcja pomocnicza do budowania idealnego drzewa binarnego
// Wypełnia je wartościami z tablicy (w kolejności poziomami)
Node* buildPerfectTree(int values[], int index, int maxIndex) {
    if (index >= maxIndex) {
        return NULL;
    }

    Node* node = createNode(values[index]);

    // Obliczamy indeksy dzieci
    int leftIndex = 2 * index + 1;
    int rightIndex = 2 * index + 2;

    node->left = buildPerfectTree(values, leftIndex, maxIndex);
    node->right = buildPerfectTree(values, rightIndex, maxIndex);

    return node;
}

// Funkcja do zwalniania pamięci drzewa
void freeTree(Node* node) {
    if (node == NULL) {
        return;
    }
    freeTree(node->left);
    freeTree(node->right);
    free(node);
}

// Funkcja do wysyłania drzewa (przejście pre-order)
// Wysyła każdy węzeł wraz z jego indeksem tablicowym
void sendTree(Node* node, int index, int sock) {
    if (node == NULL) {
        return;
    }

    // 1. Przygotuj pakiet
    Packet pkt;
    // Konwertuj na porządek sieciowy (Big-Endian)
    pkt.index = htonl(index);
    pkt.value = htonl(node->value);

    // 2. Wyślij pakiet
    if (send(sock, &pkt, sizeof(Packet), 0) < 0) {
        perror("send failed");
        return;
    }

    // Używamy ntohl tylko do lokalnego wydruku (pkt jest już w big-endian)
    printf("Wysłano węzeł: index=%d, value=%d\n", ntohl(pkt.index), ntohl(pkt.value));

    // 3. Przejdź rekurencyjnie do dzieci
    sendTree(node->left, 2 * index + 1, sock);
    sendTree(node->right, 2 * index + 2, sock);
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    // --- 1. Utwórz drzewo (15 węzłów) ---
    // Wartości do wstawienia (kolejność poziomami)
    int values[NUM_NODES] = {
        100, // 0
        50, 150, // 1, 2
        25, 75, 125, 175, // 3, 4, 5, 6
        10, 30, 60, 80, 110, 130, 160, 180 // 7-14
    };

    Node* root = buildPerfectTree(values, 0, NUM_NODES);
    printf("Utworzono drzewo binarne (15 węzłów).\n");

    // --- 2. Skonfiguruj gniazdo sieciowe ---
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Konwersja adresu IP
    if (inet_pton(AF_INET, HOST, &serv_addr.sin_addr) <= 0) {
        perror("invalid address / address not supported");
        return -1;
    }

    // --- 3. Połącz się z serwerem ---
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        return -1;
    }

    printf("Połączono z serwerem %s:%d\n", HOST, PORT);

    // --- 4. Wyślij drzewo ---
    sendTree(root, 0, sock);

    // --- 5. Zakończ ---
    printf("Wysyłanie zakończone. Zamykanie połączenia.\n");
    close(sock);
    freeTree(root);

    return 0;
}