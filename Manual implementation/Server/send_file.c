#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<inttypes.h>
#include<unistd.h>
#include<time.h>
#include<gmp.h>

#define PORT 8080
#define BUFFER_SIZE 1024
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

uint8_t sBox[] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};

uint8_t rCon[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10,
    0x20, 0x40, 0x80, 0x1B, 0x36
};

uint8_t columnTransformation[] = {
    0x02, 0x03, 0x01, 0x01,
    0x01, 0x02, 0x03, 0x01,
    0x01, 0x01, 0x02, 0x03,
    0x03, 0x01, 0x01, 0x02
};

void transposeMatrix(uint8_t* matrix){
    int i, j;
    uint8_t temp;
    for(i=0; i<4; i++){
        for(j=i+1; j<4; j++){
            temp = *(matrix + i * 4 + j);
            *(matrix + i * 4 + j) = *(matrix + j * 4 + i);
            *(matrix + j * 4 + i) = temp;
        }
    }
}

void addRoundKey(uint8_t* blockState, uint32_t* roundKey){
    int i, j;
    for(i=0; i<4; i++){
        uint8_t
            wi0 =  *(roundKey + i) >> 24,
            wi1 = (*(roundKey + i) >> 16) & 0xFF,
            wi2 = (*(roundKey + i) >> 8)  & 0xFF,
            wi3 =  *(roundKey + i)        & 0xFF;

        *(blockState + i) = *(blockState + i) ^ wi0;
        *(blockState + 4 + i) = *(blockState + 4 + i) ^ wi1;
        *(blockState + 8 + i) = *(blockState + 8 + i) ^ wi2;
        *(blockState + 12 + i) = *(blockState + 12 + i) ^ wi3;
    }
}

uint8_t gf8Multiply(uint8_t a, uint8_t b){
    uint8_t res = 0;
    while(b > 0x00){
        if(b & 1){
            res = res ^ a;
        }
        uint8_t msb = a & 0x80;
        a = a << 1;
        if(msb)
            a = a ^ 0x1B;

        b = b >> 1;
    }
    return res;
}

void mixColumn(uint8_t* blockState){
    int i, j;
    for(i=0; i<4; i++){
        uint8_t
            s0 = *(blockState + i),
            s1 = *(blockState + i + 4),
            s2 = *(blockState + i + 8),
            s3 = *(blockState + i + 12);
        for(j=0; j<4; j++){
            uint8_t
                a = gf8Multiply(s0, columnTransformation[j*4 + 0]),
                b = gf8Multiply(s1, columnTransformation[j*4 + 1]),
                c = gf8Multiply(s2, columnTransformation[j*4 + 2]),
                d = gf8Multiply(s3, columnTransformation[j*4 + 3]);
            *(blockState + j * 4 + i) = a ^ b ^ c ^ d;
        }
    }
}

void shiftRow(uint8_t* blockState){
    uint8_t temp = *(blockState + 4 + 0);
    *(blockState + 4 + 0) = *(blockState + 4 + 1);
    *(blockState + 4 + 1) = *(blockState + 4 + 2);
    *(blockState + 4 + 2) = *(blockState + 4 + 3);
    *(blockState + 4 + 3) = temp;

    temp = *(blockState + 8 + 0);
    *(blockState + 8 + 0) = *(blockState + 8 + 2);
    *(blockState + 8 + 2) = temp;

    temp = *(blockState + 8 + 1);
    *(blockState + 8 + 1) = *(blockState + 8 + 3);
    *(blockState + 8 + 3) = temp;
    
    temp = *(blockState + 12 + 3);
    *(blockState + 12 + 3) = *(blockState + 12 + 2);
    *(blockState + 12 + 2) = *(blockState + 12 + 1);
    *(blockState + 12 + 1) = *(blockState + 12 + 0);
    *(blockState + 12 + 0) = temp;
}

void subByte(uint8_t* blockState){
    int i, j;
    for(i=0; i<BLOCK_SIZE; i++)
        *(blockState + i) = sBox[*(blockState + i)];
}

uint8_t* getEncryptedFileContent(uint8_t* fileContent, uint32_t* expandedKey){
    transposeMatrix(fileContent);
    int i, j;
    addRoundKey(fileContent, expandedKey);

    
    for(i=0; i<10; i++){
        subByte(fileContent);
        shiftRow(fileContent);
        if(i < 9)
            mixColumn(fileContent);
        addRoundKey(fileContent, expandedKey + i * 4 + 4);
    }
    return fileContent;
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

uint32_t* getExpandedKey(uint8_t* uniqueKey){
    uint32_t* w = (uint32_t*)malloc(EXPANDED_KEY_SIZE * sizeof(uint32_t));
    int i;
    for(i=0; i<4; i++)
        *(w + i) =
            (*(uniqueKey + i * 4 + 0) << 24) + (*(uniqueKey + i * 4 + 1) << 16) + (*(uniqueKey + i * 4 + 2) << 8) + (*(uniqueKey + i * 4 + 3));
    
    for(i=4; i<44; i++){
        uint32_t temp = w[i-1];
        if((i%4) == 0){
            uint8_t
                b0 =  temp >> 24,
                b1 = (temp >> 16) & 0xFF,
                b2 = (temp >> 8)  & 0xFF,
                b3 =  temp        & 0xFF;
            uint8_t
                s0 = sBox[b1],
                s1 = sBox[b2],
                s2 = sBox[b3],
                s3 = sBox[b0];
            s0 = s0 ^ rCon[(i/4)-1];
            temp = (s0 << 24) + (s1 << 16) + (s2 << 8) + s3;
        }
        *(w + i) = *(w + i - 4) ^ temp;
    }
    
    return w;
}

void sendFileContent(int socket, uint8_t* fileContent, int size){
    send(socket, fileContent, size, 0);
    memset(fileContent, 0, size);
}

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

void sendFile(int socket, char* filename, uint8_t* uniqueKey){
    send(socket, filename, BUFFER_SIZE, 0);

    FILE * fp = fopen(filename, "rb");
    
    uint8_t* fileContent;
    fileContent = (uint8_t*)malloc(BLOCK_SIZE * sizeof(uint8_t));
    memset(fileContent, 0, BLOCK_SIZE);

    uint32_t* expandedKey = getExpandedKey(uniqueKey);

    int len;
    int* lenToBeSent = (int*)malloc(sizeof(int));
    while((len = fread(fileContent, sizeof(uint8_t), BLOCK_SIZE, fp)) > 0){
        uint8_t* encryptedFileContent = getEncryptedFileContent(fileContent, expandedKey);
        *(lenToBeSent) = len;
        sendFileContent(socket, encryptedFileContent, BLOCK_SIZE);
        send(socket, lenToBeSent, sizeof(int), 0);
    }
    fclose(fp);
}

int main(int argc, char *argv[]){
    srand(time(NULL));

    int server_fd, sock, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
       
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
    if ((sock = accept(server_fd, (struct sockaddr *)&address, 
                       (socklen_t*)&addrlen))<0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    clock_t begin, end;
    begin = clock();

    uint8_t* uniqueKey = generateUniqueKey();
    sendUniqueKey(sock, uniqueKey);
    sendFile(sock, argv[1], uniqueKey);
    printf("File sent successfully\n");

    end = clock();

    double deltaTime = ((double)(end-begin)) / CLOCKS_PER_SEC;
    printf("Running time: %lfs...\n", deltaTime);

    close(server_fd);
    close(sock);
    return 0;
}