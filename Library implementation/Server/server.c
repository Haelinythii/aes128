#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include "aes.h"

#define PORT 8080
#define BUFFER_SIZE 16
#define BLOCK_SIZE 16
#define EXPANDED_KEY_SIZE 44
#define KEY_PAIR_SIZE 2

int primeNumber[] = {
    11, 13, 17, 19,
    23, 29, 31, 37,
    41, 43, 47, 53,
    59, 61, 67, 71,
    73, 79, 83, 89,
    97,
};

int gcd(int a, int b)
{
	// Everything divides 0
	if (a == 0)
	return b;
	if (b == 0)
	return a;

	// base case
	if (a == b)
		return a;

	// a is greater
	if (a > b)
		return gcd(a-b, b);
	return gcd(a, b-a);
}

int findE(int phi){
    return phi-1;
}

int findD(int e, int phi){
    int i;
    int offset = rand()%e + 1;
    for(i=phi/2 + offset; i<phi; i++){
        int temp = i * e;
        temp = temp % phi;
        if(temp == 1)
            return i;
    }
    return e;
}

int modExp(int a, int pow, int mod)
{
	int res = 1;

	a = a % mod;

	if (a == 0)
        return 0;

	while (pow > 0)
	{
		if (pow & 1)
			res = (res * a) % mod;

		pow = pow>>1;
		a = (a*a) % mod;
	}
	return res;
}

uint8_t* generateUniqueKey() {
    uint8_t* uniqueKey = (uint8_t*)malloc(BLOCK_SIZE * sizeof(uint8_t));
    printf("Generated AES key:");
    for (uint8_t i = 0; i < BLOCK_SIZE; i++) {
        uint8_t temp = rand() % 256;
        *(uniqueKey + i) = temp;
        printf(" %x", temp);
    }
    printf("\n");

    return uniqueKey;
}

void sendUniqueKey(int socket, uint8_t* uniqueKey){
    int* encryptedUniqueKey = (int*)malloc(BLOCK_SIZE * sizeof(int));
    int a, b, index;
    
    index = rand() % 12;
    a = primeNumber[index];
    index += rand() % 10 + 1;
    b = primeNumber[index];
    printf("Generated prime numbers: %d %d\n", a, b);
    int
        n = a * b,
        phi = (a-1) * (b-1);
    
    int e = findE(phi);
    int d = findD(e, phi);

    int i;
    for(i=0; i<BLOCK_SIZE; i++){
        int temp = *(uniqueKey + i);
        *(encryptedUniqueKey + i) = modExp(temp, e, n);
    }
    int* keyPair = (int*)malloc(KEY_PAIR_SIZE * sizeof(int));
    *(keyPair) = d;
    *(keyPair + 1) = n;
    send(socket, keyPair, KEY_PAIR_SIZE * sizeof(int), 0);
    send(socket, encryptedUniqueKey, BLOCK_SIZE * sizeof(int), 0);
}

void sendEncryptedFileContent(int socket, uint8_t* fileContent, int size){
    send(socket, fileContent, size, 0);
    memset(fileContent, 0, size);
}

void sendFile(int socket, char* filename, uint8_t* uniqueKey){
    send(socket, filename, strlen(filename), 0);

    FILE * fp = fopen(filename, "rb");

    struct AES_ctx aes_ctx;
    AES_init_ctx(&aes_ctx, uniqueKey);

    uint8_t* fileContent;
    fileContent = (uint8_t*)malloc(BUFFER_SIZE * sizeof(uint8_t));
    memset(fileContent, 0, BUFFER_SIZE);

    int len;
    int* lenToBeSent = (int*)malloc(sizeof(int));
    while((len = fread(fileContent, sizeof(uint8_t), BLOCK_SIZE, fp)) > 0){
        AES_ECB_encrypt(&aes_ctx, fileContent);
        

        *(lenToBeSent) = len;
        sendEncryptedFileContent(socket, fileContent, BLOCK_SIZE);
        send(socket, lenToBeSent, sizeof(int), 0);
    }
    fclose(fp);
}

int main(int argc, char *argv[]){
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE], anotherBuffer[BUFFER_SIZE];
       
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
       
    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
       
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, 
                                 sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, 
                       (socklen_t*)&addrlen))<0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    clock_t begin, end;
    begin = clock();

    uint8_t* uniqueKey = generateUniqueKey();
    sendUniqueKey(new_socket, uniqueKey);
    sendFile(new_socket, argv[1], uniqueKey);
    
    end = clock();

    double deltaTime = ((double)(end-begin)) / CLOCKS_PER_SEC;
    printf("Running time: %lfs...\n", deltaTime);
    printf("File sent successfully\n");
    return 0;
}