#include<stdio.h>
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
#include<time.h>
#include<stdlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include<windows.h>
#include<mswsock.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
#define SHUTDOWN_BOTH SD_BOTH
#define SEP '\\'
typedef BOOL (WINAPI *PFN_TRANSMITFILE)(
    SOCKET hSocket,
    HANDLE hFile,
    DWORD nNumberOfBytesToWrite,
    DWORD nNumberOfBytesPerSend,
    LPOVERLAPPED lpOverlapped,
    LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers,
    DWORD dwFlags
);
#else
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/tcp.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<unistd.h>
#define SHUTDOWN_BOTH SHUT_RDWR
#define SEP '/'
#endif
#define MAX_BUF 256
#define PORT 4000
#define MAX_NAME MAX_BUF
#define MAX_PEERS 8
#define TIMEOUT 3
#define STR(x) #x
#define XSTR(x) STR(x)
#define PORT_STRING XSTR(PORT)
#define BUF_SIZE (1 << 16)
#define METADATA (sizeof(uint64_t) +sizeof(uint32_t) + 1)
typedef struct Metadata{
    uint64_t size;
    char is_file;
    char* path;
} Metadata;
typedef struct Broadcast{
    struct sockaddr_storage* addr;
    char* name;
    time_t time;
    bool (*is_old)(struct Broadcast*);
}Broadcast;
typedef struct BroadcastList{
    Broadcast broadcasts[MAX_PEERS];
    int size;
    int (*add)(struct BroadcastList*,struct sockaddr_storage, char*);
    int (*clean)(struct BroadcastList*);
    int (*list_free)(struct BroadcastList*);
} BroadcastList;

void print_error(char* msg);
void window_startup();
bool is_old(Broadcast* self);
bool check_broadcast_same(struct sockaddr_storage* addr1,struct sockaddr_storage* addr2);
int add(BroadcastList* self,struct sockaddr_storage addr, char* name);
int clean(BroadcastList* self);
int list_free(BroadcastList* self);
BroadcastList* list_constructor();
int close_socket(int socket);