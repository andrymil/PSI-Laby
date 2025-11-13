#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // close()
#include <arpa/inet.h>  // htonl()
#include <sys/socket.h> // socket()
#include <netdb.h>      // <-- POTRZEBNE DO ROZWIĄZYWANIA NAZW

// --- POPRAWKA 1 ---
// Używamy nazwy USŁUGI 'server' z pliku docker-compose.yml
// Docker (w sieci z34_network) sam zamieni to na właściwy adres IP
#define HOST "server"
#define PORT 8888
#define PORT_STR "8888" // Port jako string dla getaddrinfo
#define NUM_NODES 15

// (Struktury Node, Packet oraz funkcje createNode,
// buildPerfectTree, freeTree, sendTree pozostają BEZ ZMIAN)

typedef struct Node {
    int value;
    struct Node *left;
    struct Node *right;
} Node;

#pragma pack(push, 1)
typedef struct {
    int32_t index;
    int32_t value;
} Packet;
#pragma pack(pop)

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

Node* buildPerfectTree(int values[], int index, int maxIndex) {
    if (index >= maxIndex) {
        return NULL;
    }
    Node* node = createNode(values[index]);
    int leftIndex = 2 * index + 1;
    int rightIndex = 2 * index + 2;
    node->left = buildPerfectTree(values, leftIndex, maxIndex);
    node->right = buildPerfectTree(values, rightIndex, maxIndex);
    return node;
}

void freeTree(Node* node) {
    if (node == NULL) return;
    freeTree(node->left);
    freeTree(node->right);
    free(node);
}

void sendTree(Node* node, int index, int sock) {
    if (node == NULL) return;
    Packet pkt;
    pkt.index = htonl(index);
    pkt.value = htonl(node->value);

    if (send(sock, &pkt, sizeof(Packet), 0) < 0) {
        perror("send failed");
        return;
    }
    printf("Wysłano węzeł: index=%d, value=%d\n", ntohl(pkt.index), ntohl(pkt.value));
    sendTree(node->left, 2 * index + 1, sock);
    sendTree(node->right, 2 * index + 2, sock);
}


// --- POPRAWKA 2 ---
// Funkcja pomocnicza do rozwiązywania nazwy hosta i łączenia
int connectToServer() {
    int sock = -1;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // Wymuszamy IPv4 (zgodnie z instrukcją)
    hints.ai_socktype = SOCK_STREAM;

    // Rozwiąż nazwę hosta (HOST) na adres IP
    if ((rv = getaddrinfo(HOST, PORT_STR, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        return -1;
    }

    // Przejdź przez wyniki i połącz się z pierwszym możliwym
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock);
            perror("client: connect");
            continue;
        }
        break; // Udało się połączyć
    }

    if (p == NULL) {
        fprintf(stderr, "Nie udało się połączyć z hostem %s\n", HOST);
        sock = -1;
    }

    freeaddrinfo(servinfo); // Zwolnij pamięć
    return sock;
}
// --------------------

int main() {
    int sock = 0;

    int values[NUM_NODES] = {
        100, 50, 150, 25, 75, 125, 175,
        10, 30, 60, 80, 110, 130, 160, 180
    };

    Node* root = buildPerfectTree(values, 0, NUM_NODES);
    printf("Utworzono drzewo binarne (15 węzłów).\n");

    // --- POPRAWKA 3 ---
    // Używamy nowej funkcji do łączenia
    printf("Łączenie z serwerem %s:%d...\n", HOST, PORT);
    sock = connectToServer();

    if (sock == -1) {
        fprintf(stderr, "Błąd krytyczny podczas łączenia z serwerem.\n");
        freeTree(root);
        return -1;
    }
    // --------------------

    printf("Połączono z serwerem %s:%d\n", HOST, PORT);

    sendTree(root, 0, sock);

    printf("Wysyłanie zakończone. Zamykanie połączenia.\n");
    close(sock);
    freeTree(root);

    return 0;
}