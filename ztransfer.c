#define WIN32_LEAN_AND_MEAN
#include<windows.h>
#include<mswsock.h>
#include<stdio.h>
#include<string.h>
#include<time.h>
#include<stdbool.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
#define MAX_BUF 256
#define PORT 4000
#define MAX_NAME MAX_BUF
#define MAX_PEERS 8
#define TIMEOUT 3
#define STR(x) #x
#define XSTR(x) STR(x)
#define PORT_STRING XSTR(PORT)
#define BUF_SIZE (1 << 16)

typedef BOOL (WINAPI *PFN_TRANSMITFILE)(
    SOCKET hSocket,
    HANDLE hFile,
    DWORD nNumberOfBytesToWrite,
    DWORD nNumberOfBytesPerSend,
    LPOVERLAPPED lpOverlapped,
    LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers,
    DWORD dwFlags
);

typedef struct Broadcast{
    struct sockaddr_storage addr;
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
bool is_old(Broadcast* self){
    return time(NULL) - self->time > TIMEOUT;
}
bool check_broadcast_same(struct sockaddr_storage* addr1,struct sockaddr_storage* addr2){
    return memcmp((&((struct sockaddr_in*)addr1)->sin_addr.S_un.S_addr),&(((struct sockaddr_in*)addr2)->sin_addr.S_un.S_addr), sizeof(((struct sockaddr_in*)addr2)->sin_addr.S_un.S_addr));
}
int add(BroadcastList* self,struct sockaddr_storage addr, char* name){
    for (int i = 0;i < self->size;i++){
        if(check_broadcast_same(&self->broadcasts[i].addr,&addr) == 0){
            self->broadcasts[i].time = time(NULL);
            return 0;
        }
    }
    if (self->size < MAX_PEERS){
        Broadcast broadcast = {addr,name,time(NULL),is_old};
        self->broadcasts[self->size++] = broadcast;
        return 0;
    }
    return -1;
}
int clean(BroadcastList* self){
    for (int i = 0;i < self->size;i++){
        if (self->broadcasts[i].is_old(&self->broadcasts[i])){
            memmove(&self->broadcasts[i],&self->broadcasts[i+1],self->size - i - 1);
            self->size--;
        }
    }
    return 0;
}
int list_free(BroadcastList* self){
    for (int i = 0;i < self->size;i++){
        free(self->broadcasts[i].name);
    }
    free(self);
    return 0;
}
BroadcastList* list_constructor(){
    BroadcastList* list = malloc(sizeof(BroadcastList));
    list->size = 0;
    list->clean = clean;
    list->list_free = list_free;
    list->add = add;
    return list;
}
void print_error(char* msg){
    #ifdef _WIN32
    printf("%s: %d\n",msg,WSAGetLastError());
    #else
    printf("%s",msg);
    perror("");
    #endif
}
void window_startup(){
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2,2),&wsadata) !=0){
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }
    if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2){
        fprintf(stderr, "version 2.2 of WinSock not available.\n");
        WSACleanup();
        exit(2);
    }
}
int get_broadcast_socket(){
    int sockfd, broadcast = 1;
    if ((sockfd = socket(AF_INET,SOCK_DGRAM,0))==-1){
        print_error("socket");
        exit(1);
    }
    if(setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,(char *)&broadcast,sizeof broadcast)==-1){
        print_error("socket options(broadcast)");
        exit(1);
    }
    return sockfd;
}
int send_udp_packet(int sockfd,struct sockaddr_in* broadcast_addr,char* msg,int size){
    if (sendto(sockfd,msg,size,0,(struct sockaddr*)broadcast_addr, sizeof(struct sockaddr_in))==-1){
        print_error("sendto");
        exit(1);
    }
    return 0;
}
int get_udp_listener(){
    struct addrinfo info, *res, *p;
    memset(&info,0,sizeof info);
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_DGRAM;
    info.ai_flags = AI_PASSIVE;
    int status;
    if ((status = getaddrinfo(NULL,PORT_STRING,&info,&res) !=0)){
        fprintf(stderr,"gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    int sockfd;
    for(p = res; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
            continue;
        }
        if (bind(sockfd,p->ai_addr, p->ai_addrlen)== -1){
            closesocket(sockfd);
            continue;
        }
        break;
    }
    if (p == NULL){
        printf("could not bind");
        exit(1);
    }
    freeaddrinfo(res);
    return sockfd;
}
int get_tcp_listener(){
    struct addrinfo info, *res, *p;
    memset(&info,0,sizeof info);
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;
    info.ai_flags = AI_PASSIVE;
    int status;
    if ((status = getaddrinfo(NULL,PORT_STRING,&info,&res)) !=0){
        fprintf(stderr,"gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    int sockfd;
    int yes = 1;
    int size = 1 << 20;
    for(p = res; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(const char*)&yes,sizeof(int)) == -1){
            printf("error in sockopt");
            closesocket(sockfd);
            exit(1);
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF,(const char*)&size,sizeof(int)) == -1){
            printf("error in sockopt");
            closesocket(sockfd);
            exit(1);
        }
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,(const char*)&yes,sizeof(int)) == -1){
            printf("error in sockopt");
            closesocket(sockfd);
            exit(1);
        }
        if (bind(sockfd,p->ai_addr, p->ai_addrlen)== -1){
            closesocket(sockfd);
            continue;
        }
        break;
    }
    if (p == NULL){
        printf("could not bind");
        exit(1);
    }
    freeaddrinfo(res);
    return sockfd;
}
int get_tcp_socket(struct sockaddr_storage* their_addr){
    int tcp_fd;
    struct sockaddr_in connect_addr;
    memset(&connect_addr,0,sizeof connect_addr);
    connect_addr.sin_family = ((struct sockaddr_in*)their_addr)->sin_family;
    connect_addr.sin_addr = ((struct sockaddr_in*)their_addr)->sin_addr;    
    connect_addr.sin_port = htons(PORT);
    int size = 1 << 20;
    if ((tcp_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){
        print_error("socket");
        exit(1);
    }
    if (setsockopt(tcp_fd, SOL_SOCKET, SO_RCVBUF,(const char*)&size,sizeof(int)) == -1){
            printf("error in sockopt");
            closesocket(tcp_fd);
            exit(1);
        }
    if (connect(tcp_fd, (struct sockaddr*)&connect_addr,sizeof connect_addr) == -1){
        print_error("connect");
        exit(1);
    }
    return tcp_fd;
}
int discovery(){
    fd_set my_set;
    int tcp_fd = get_tcp_listener();
    int udp_fd = get_broadcast_socket();
    struct sockaddr_storage their_addr;
    struct sockaddr_in broadcast_addr;
    int size = sizeof their_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    struct timeval time;
    int new_fd;
    char* hostname = malloc(MAX_NAME);
    if (gethostname(hostname,MAX_NAME) == -1){
        print_error("host name");
        exit(1);
    }
    printf("my name is %s\n",hostname);
    int name_len = strlen(hostname);
    inet_pton(AF_INET,"192.168.1.255",&(broadcast_addr.sin_addr));
    if (listen(tcp_fd,1)==-1){
        printf("could not listen");
        exit(1);
    }
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    while(1){
        FD_ZERO(&my_set);
        FD_SET(tcp_fd,&my_set);
        time.tv_sec = 0;
        time.tv_usec = 10000;
        send_udp_packet(udp_fd,&broadcast_addr,hostname,name_len);
        select(tcp_fd + 1,&my_set,NULL, NULL,&time);
        if(FD_ISSET(tcp_fd,&my_set)){
            if ((new_fd = accept(tcp_fd,(struct sockaddr*)&their_addr,& size)) == -1){
                print_error("accept");
                continue;
            }
            break;
        }
        // if(WaitForSingleObject(h,990)== WAIT_OBJECT_0){
        //     char c = getchar();
        //     printf("i read that: %c\n", c);
        // }
    }
    closesocket(udp_fd);
    closesocket(tcp_fd);
    return new_fd;
}
int listen_to_discovery(){
    fd_set my_set;
    char buf[MAX_BUF];
    int bytes;
    int udp_fd = get_udp_listener();
    int tcp_fd;
    struct sockaddr_storage their_addr;
    int size = sizeof their_addr;
    struct timeval time;
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    BroadcastList* list = list_constructor();
    while(1){
        FD_ZERO(&my_set);
        FD_SET(udp_fd,&my_set);
        time.tv_sec = 0;
        time.tv_usec = 10000;
        select(udp_fd+1,&my_set,NULL,NULL,&time);
        if(FD_ISSET(udp_fd,&my_set)){
            if ((bytes = recvfrom(udp_fd,buf,MAX_BUF,0,(struct sockaddr*)&their_addr,&size)) == -1){
                print_error("recv");
                continue;
            }
            buf[bytes] = '\0';
            char* name = malloc(bytes+1);
            strcpy(name,buf);
            list->add(list,their_addr,name);
            printf("bytes recv: %s\n", buf);
            for(int i = 0; i < list->size;i++){
                printf("name: %s\n",list->broadcasts[i].name);
            }
            break;
        }
        //list->clean(list);
        // if(WaitForSingleObject(h,990) == WAIT_OBJECT_0){
        //     char c = getchar();
        //     printf("i read that: %c\n", c);
        //     if(c == 'q')
        //         break;
        // }
    }
    closesocket(udp_fd);
    list->list_free(list);
    tcp_fd = get_tcp_socket(&their_addr);
    return tcp_fd;
}
int send_file(int tcp_fd){
    double elapsed_seconds;
    LARGE_INTEGER freq, start, end;
    PFN_TRANSMITFILE pTransmitFile;
    pTransmitFile = (PFN_TRANSMITFILE)GetProcAddress(
        GetModuleHandleW(L"mswsock.dll"), 
        "TransmitFile"
    );
    HANDLE file = CreateFile("testfile.txt",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printf("CreateFile failed: %lu\n", GetLastError());
        return 0;
    }
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    bool ok = pTransmitFile(tcp_fd,file,0,0,NULL,NULL,0);
    QueryPerformanceCounter(&end);
    elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
    double speed = 160 / elapsed_seconds;
    printf("time send %.9fs\n average speed %.9fMB/s\n",elapsed_seconds,speed);
    if(!ok){
        print_error("transmit file");
    }
    return 0;
}
int receive_file(int tcp_fd){
    FILE* file = fopen("testfile.txt","w");
    char buf[BUF_SIZE];
    double elapsed_seconds;
    int bytes;
    LARGE_INTEGER freq, start, end;
    // QueryPerformanceFrequency(&freq);
    // QueryPerformanceCounter(&start);
    while (true){
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        bytes = recv(tcp_fd,buf,BUF_SIZE,0);
        QueryPerformanceCounter(&end);
        elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
        printf("time recv %.9fs\n",elapsed_seconds);
        if(bytes == 0){
            break;
        }
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        fwrite(buf,sizeof(char),bytes,file);
        QueryPerformanceCounter(&end);
        elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
        printf("time write %.9fs\n",elapsed_seconds);
    }
    // QueryPerformanceCounter(&end);
    // elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
    // double speed = 160 / elapsed_seconds;
    // printf("time recv %.9fs\n average speed %.9fMB/s\n",elapsed_seconds,speed);
    fclose(file);
    return 0;
}
int main(int argc, char* argv[]){
    if (argc != 2){
        printf("expected argument ./ztransfer [send | receive]\n");
    }
    window_startup();
    int tcp_fd;
    if (strcmp(argv[1],"broadcast") == 0){
        tcp_fd = discovery();
        send_file(tcp_fd);
        shutdown(tcp_fd,SD_BOTH);
    }
    else if (strcmp(argv[1],"listen") == 0){
        tcp_fd = listen_to_discovery();
        receive_file(tcp_fd);
        shutdown(tcp_fd,SD_BOTH);
    }
    else{
        printf("invalid argument\n");
        exit(1);
    }
}