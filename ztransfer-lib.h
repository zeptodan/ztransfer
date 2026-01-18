#include<stdio.h>
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
#include<time.h>
#include<stdlib.h>
#ifdef DEBUG
#ifdef _WIN32
#define START_TIMER(name) double name##_elapsed_seconds; \
    LARGE_INTEGER name##_freq, name##_start, name##_end; \
    QueryPerformanceFrequency(&name##_freq); \
    QueryPerformanceCounter(&name##_start);
#define END_TIMER(name) QueryPerformanceCounter(&name##_end); \
    name##_elapsed_seconds = ((double)(name##_end.QuadPart - name##_start.QuadPart) / name##_freq.QuadPart); \
    double name##_speed = 160 / name##_elapsed_seconds; \
    printf("time recv %.9fs\n average speed %.9fMB/s\n",name##_elapsed_seconds,name##_speed);
#else
#define START_TIMER(name) double name##_elapsed_seconds; \
    struct timespec name##_start, name##_end; \
    clock_gettime(CLOCK_MONOTONIC, &name##_start);
#define END_TIMER(name) clock_gettime(CLOCK_MONOTONIC, &name##_end); \
    name##_elapsed_seconds = (((name##_end.tv_sec - name##_start.tv_sec) * 1000000000ULL)+(name##_end.tv_nsec - name##_start.tv_nsec)) / 1e9; \
    double name##_speed = 160 / name##_elapsed_seconds; \
    printf("time recv %.9fs\n average speed %.9fMB/s\n",name##_elapsed_seconds,name##_speed);
#endif
#else
#define START_TIMER(name)
#define END_TIMER(name)
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include<windows.h>
#include<mswsock.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
#define SHUTDOWN_BOTH SD_BOTH
#define SEP '\\'
#define my_ntohll(number) ntohll(number)
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
#include<sys/stat.h>
#include<sys/sendfile.h>
#include<sys/select.h>
#include<netinet/in.h>
#include<netinet/tcp.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<endian.h>
#include<dirent.h>
#include<fcntl.h>
#define SHUTDOWN_BOTH SHUT_RDWR
#define SEP '/'
#define my_ntohll(number) htobe64(number)
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
typedef struct Broadcast_addrs{
    int size;
    char** addrs;
    int (*add)(struct Broadcast_addrs,char* addr);
    int (*broadcast)(struct Broadcast_addrs);
} Broadcast_addrs;
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
uint64_t get_file_size(char* path);
int create_folder(char* path);
int send_file(int tcp_fd,char* path);
int send_folder(int tcp_fd,char* path);
int send_metadata(char is_file,int tcp_fd,char* path);
bool is_folder(char* path);