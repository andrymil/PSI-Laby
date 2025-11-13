#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define HOST "server"
#define PORT 8888
#define PORT_STR "8888"
#define NUM_NODES 15

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
    printf("Sent node: index=%d, value=%d\n", ntohl(pkt.index), ntohl(pkt.value));
    sendTree(node->left, 2 * index + 1, sock);
    sendTree(node->right, 2 * index + 2, sock);
}

int connectToServer() {
    int sock = -1;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(HOST, PORT_STR, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        return -1;
    }

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
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to connect to host %s\n", HOST);
        sock = -1;
    }

    freeaddrinfo(servinfo);
    return sock;
}

int main() {
    int sock = 0;

    int values[NUM_NODES] = {
        100, 50, 150, 25, 75, 125, 175,
        10, 30, 60, 80, 110, 130, 160, 180
    };

    Node* root = buildPerfectTree(values, 0, NUM_NODES);
    printf("Created binary tree (15 nodes).\n");

    printf("Connecting to server %s:%d...\n", HOST, PORT);
    sock = connectToServer();

    if (sock == -1) {
        fprintf(stderr, "Critical error while connecting to server.\n");
        freeTree(root);
        return -1;
    }

    printf("Connected to server %s:%d\n", HOST, PORT);

    sendTree(root, 0, sock);

    printf("Sending complete. Closing connection.\n");
    close(sock);
    freeTree(root);

    return 0;
}