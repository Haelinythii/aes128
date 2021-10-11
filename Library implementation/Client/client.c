#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<inttypes.h>
#include<unistd.h>
#include<time.h>
#include "aes.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define BLOCK_SIZE 16
#define EXPANDED_KEY_SIZE 44
#define KEY_PAIR_SIZE 2

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

uint8_t* recvUniqueKey(int socket){
    int* keyPair = (int*)malloc(KEY_PAIR_SIZE * sizeof(int));
    int* encryptedUniqueKey = (int*)malloc(BLOCK_SIZE * sizeof(int));

    recv(socket, keyPair, KEY_PAIR_SIZE * sizeof(int), 0);
    recv(socket, encryptedUniqueKey, BLOCK_SIZE * sizeof(int), 0);

    uint8_t* uniqueKey = (uint8_t*)malloc(BLOCK_SIZE * sizeof(uint8_t));
    int i;
    for(i=0; i<BLOCK_SIZE; i++)
        *(uniqueKey + i) = modExp(*(encryptedUniqueKey + i), *(keyPair), *(keyPair + 1));
    return uniqueKey;
}

void recvFile(int socket, char* filename, uint8_t* uniqueKey){
    recv(socket, filename, BUFFER_SIZE, 0);
    FILE * fp = fopen(filename, "wb");

    struct AES_ctx aes_ctx;
    AES_init_ctx(&aes_ctx, uniqueKey);
    
    uint8_t* fileContent;
    fileContent = (uint8_t*)malloc(BUFFER_SIZE * sizeof(uint8_t));
    memset(fileContent, 0, BUFFER_SIZE);

    int* len = (int*)malloc(sizeof(int));
    while(recv(socket, fileContent, BLOCK_SIZE, 0) > 0){
        recv(socket, len, sizeof(int), 0);
        AES_ECB_decrypt(&aes_ctx, fileContent);

        fwrite(fileContent, sizeof(uint8_t), *(len), fp);

        memset(fileContent, 0, BLOCK_SIZE);
    }
    fclose(fp);
    printf("Received file: %s\n", filename);
}

int main(int argc, char const *argv[])
{
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char filename[BUFFER_SIZE];
    memset(filename, 0, BUFFER_SIZE);
    uint8_t buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
       
    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    clock_t begin, end;
    begin = clock();

    uint8_t* uniqueKey = recvUniqueKey(sock);
    recvFile(sock, filename, uniqueKey);

    end = clock();

    double deltaTime = ((double)(end-begin)) / CLOCKS_PER_SEC;
    printf("Running time: %lfs...\n", deltaTime);

    return 0;
}