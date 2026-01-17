#include"ztransfer-lib.h"
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
char buf[BUF_SIZE];
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
            close_socket(sockfd);
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
            close_socket(sockfd);
            exit(1);
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF,(const char*)&size,sizeof(int)) == -1){
            printf("error in sockopt");
            close_socket(sockfd);
            exit(1);
        }
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,(const char*)&yes,sizeof(int)) == -1){
            printf("error in sockopt");
            close_socket(sockfd);
            exit(1);
        }
        if (bind(sockfd,p->ai_addr, p->ai_addrlen)== -1){
            close_socket(sockfd);
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
            close_socket(tcp_fd);
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
    inet_pton(AF_INET,"192.168.137.255",&(broadcast_addr.sin_addr));
    if (listen(tcp_fd,1)==-1){
        printf("could not listen");
        exit(1);
    }
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
    }
    close_socket(udp_fd);
    close_socket(tcp_fd);
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
    }
    close_socket(udp_fd);
    list->list_free(list);
    tcp_fd = get_tcp_socket(&their_addr);
    return tcp_fd;
}
int send_metadata(char is_file,int tcp_fd,char* path){
    static int abs_len = -1;
    if(abs_len == -1){
        abs_len = strlen(path);
    }
    *buf = is_file;
    if (is_file){
        WIN32_FILE_ATTRIBUTE_DATA file_data;
        GetFileAttributesExA(path, GetFileExInfoStandard, &file_data);
        uint64_t size = htonll(((uint64_t)file_data.nFileSizeHigh << 32) | file_data.nFileSizeLow);
        memcpy(buf + 1,&size,sizeof(uint64_t));
    }
    uint32_t len = strlen(path)-abs_len;
    uint32_t len_network = htonl(len);
    memcpy(buf + sizeof(uint64_t) + 1,&len_network,sizeof(uint32_t));
    strcpy(buf + sizeof(uint64_t) + sizeof(uint32_t) + 1,path + abs_len);
    uint32_t size = len + sizeof(uint64_t) + sizeof(uint32_t) + 1;
    send(tcp_fd,buf,size,0);
}
int send_file(int tcp_fd,char* path){
    // char buf[BUF_SIZE];
    // int bytes;
    // int read;
    // START_TIMER(timer1);
    // FILE* file = fopen("testfile.txt","r");
    // while (true){
    //     // QueryPerformanceFrequency(&freq);
    //     // QueryPerformanceCounter(&start);
    //     int read = fread(buf,sizeof(char),BUF_SIZE,file);
    //     if (read == 0){
    //         break;
    //     }
    //     bytes = send(tcp_fd,buf,read,0);
    //     // QueryPerformanceCounter(&end);
    //     // elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
    //     // printf("time recv %.9fs\n",elapsed_seconds);
    //     // printf("bytes %f\n",bytes / 1024.0);
    //     // QueryPerformanceFrequency(&freq);
    //     // QueryPerformanceCounter(&start);
    //     // fwrite(buf,sizeof(char),bytes,file);
    //     // QueryPerformanceCounter(&end);
    //     // elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
    //     // printf("time write %.9fs\n",elapsed_seconds);
    // }
    // END_TIMER(timer1);
    send_metadata(1,tcp_fd,path);

    u_long mode = 1;
    ioctlsocket(tcp_fd, FIONBIO, &mode);
    PFN_TRANSMITFILE pTransmitFile;
    pTransmitFile = (PFN_TRANSMITFILE)GetProcAddress(
        GetModuleHandleW(L"mswsock.dll"), 
        "TransmitFile"
    );
    HANDLE file = CreateFile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printf("CreateFile failed: %lu\n", GetLastError());
        return 0;
    }
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    START_TIMER(timer1);
    bool ok = pTransmitFile(tcp_fd,file,0,0,&ov,NULL,TF_USE_KERNEL_APC);
    WaitForSingleObject(ov.hEvent, INFINITE);
    DWORD ignored;
    GetOverlappedResult((HANDLE)tcp_fd, &ov, &ignored, FALSE);
    END_TIMER(timer1);
    if(!ok){
        print_error("transmit file");
        exit(1);
    }
    return 0;
}
int send_folder(int tcp_fd,char* path){
    send_metadata(0,tcp_fd,path);
    WIN32_FIND_DATAA f;
    HANDLE h = FindFirstFileA(path,&f);
    char* path_with_name;
    do {
        if (!strcmp(f.cFileName, ".") || !strcmp(f.cFileName, ".."))
            continue;
        path_with_name = malloc(strlen(path) + strlen(f.cFileName) + 2);
        strcpy(path_with_name,path);
        path_with_name[strlen(path)] = SEP;
        strcpy(path_with_name + strlen(path) + 1,f.cFileName);
        if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            send_folder(tcp_fd,path_with_name);
        }
        else{
            send_file(tcp_fd,path_with_name);
        }
        free(path_with_name);
    }while(FindNextFileA(h,&f));
}
Metadata receive_metadata(int tcp_fd){
    int read = 0;
    while (read != METADATA){
        read += recv(tcp_fd,buf,METADATA - read,0);
        if (read != 0 && buf[0] == 2){
            Metadata data = {0,2,NULL};
            return data;
        }
        if (read <= 0){
            print_error("recv metadata");
            break;
        }
    }
    char is_file = *buf;
    uint64_t size;
    memcpy(&size,buf + 1, sizeof(uint64_t));
    size = ntohll(size);
    uint32_t len;
    memcpy(&len,buf + sizeof(uint64_t) + 1,sizeof(uint32_t));
    read = 0;
    while (read != len){
        read += recv(tcp_fd,buf,len - read,0);
    }
    char* path = malloc(len + 1);
    buf[len] = '\0';
    strcpy(path,buf);
    Metadata data = {size, is_file, path};
    return data;
}
int receive_file(int tcp_fd,uint64_t size,char* path){
    FILE* file = fopen(path,"w");
    uint64_t tot_bytes = 0;
    int bytes;
    START_TIMER(timer1);
    while (tot_bytes !=size){
        // QueryPerformanceFrequency(&freq);
        // QueryPerformanceCounter(&start);
        bytes = recv(tcp_fd,buf,BUF_SIZE,0);
        // QueryPerformanceCounter(&end);
        // elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
        // printf("time recv %.9fs\n",elapsed_seconds);
        // printf("bytes %f\n",bytes / 1024.0);
        if(bytes == 0){
            break;
        }
        // QueryPerformanceFrequency(&freq);
        // QueryPerformanceCounter(&start);
        fwrite(buf,sizeof(char),bytes,file);
        tot_bytes += bytes;
        // QueryPerformanceCounter(&end);
        // elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
        // printf("time write %.9fs\n",elapsed_seconds);
    }
    END_TIMER(timer1);
    fclose(file);
    return 0;
}
int receive_folder(int tcp_fd,char* path){
    char* path_with_name;
    int len = strlen(path);
    while(true){
        Metadata data = receive_metadata(tcp_fd);
        if(data.is_file == 2){
            break;
        }
        path_with_name = malloc(len + strlen(data.path) + 2);
        strcpy(path_with_name,path);
        path_with_name[len] = SEP;
        strcpy(path_with_name + len + 1,data.path);
        if(data.is_file == 1){
            receive_file(tcp_fd,data.size,path_with_name);
        }
        else{
            CreateDirectoryA(data.path,NULL);
        }
        free(path_with_name);
    }
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
        DWORD attr = GetFileAttributesA(argv[2]);
        if (attr == INVALID_FILE_ATTRIBUTES){
            exit(1);
        }
        if (attr & FILE_ATTRIBUTE_DIRECTORY){
            send_folder(tcp_fd,argv[2]);
        }
        else{
            send_file(tcp_fd,argv[2]);
        }
        char stop = 2;
        send(tcp_fd,&stop,1,0);
        shutdown(tcp_fd,SHUTDOWN_BOTH);
    }
    else if (strcmp(argv[1],"listen") == 0){
        tcp_fd = listen_to_discovery();
        receive_folder(tcp_fd,argv[2]);
        shutdown(tcp_fd,SHUTDOWN_BOTH);
    }
    else{
        printf("invalid argument\n");
        exit(1);
    }
}