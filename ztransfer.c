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
    printf("time recv %lds\n average speed %.9fMB/s\n",name##_elapsed_seconds,name##_speed);
#endif
#else
#define START_TIMER(name)
#define END_TIMER(name)
#endif
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
    inet_pton(AF_INET,"192.168.1.255",&(broadcast_addr.sin_addr));
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
int send_file(int tcp_fd){
    char buf[BUF_SIZE];
    int bytes;
    START_TIMER(timer1);
    int i = 0;
    while (i < 2560){
        // QueryPerformanceFrequency(&freq);
        // QueryPerformanceCounter(&start);
        bytes = send(tcp_fd,buf,BUF_SIZE,0);
        // QueryPerformanceCounter(&end);
        // elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
        // printf("time recv %.9fs\n",elapsed_seconds);
        // printf("bytes %f\n",bytes / 1024.0);
        i++;
        // QueryPerformanceFrequency(&freq);
        // QueryPerformanceCounter(&start);
        // fwrite(buf,sizeof(char),bytes,file);
        // QueryPerformanceCounter(&end);
        // elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
        // printf("time write %.9fs\n",elapsed_seconds);
    }
    END_TIMER(timer1);
    // u_long mode = 1;
    // ioctlsocket(tcp_fd, FIONBIO, &mode);
    // double elapsed_seconds;
    // LARGE_INTEGER freq, start, end;
    // PFN_TRANSMITFILE pTransmitFile;
    // pTransmitFile = (PFN_TRANSMITFILE)GetProcAddress(
    //     GetModuleHandleW(L"mswsock.dll"), 
    //     "TransmitFile"
    // );
    // HANDLE file = CreateFile("testfile.txt",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    // if (file == INVALID_HANDLE_VALUE) {
    //     printf("CreateFile failed: %lu\n", GetLastError());
    //     return 0;
    // }
    // OVERLAPPED ov = {0};
    // ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    // QueryPerformanceFrequency(&freq);
    // QueryPerformanceCounter(&start);

    // bool ok = pTransmitFile(tcp_fd,file,0,0,&ov,NULL,TF_USE_KERNEL_APC);
    // printf("something\n");
    // WaitForSingleObject(ov.hEvent, INFINITE);
    // DWORD ignored;
    // GetOverlappedResult((HANDLE)tcp_fd, &ov, &ignored, FALSE);

    // QueryPerformanceCounter(&end);
    // elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
    // double speed = 160 / elapsed_seconds;
    // printf("time send %.9fs\n average speed %.9fMB/s\n",elapsed_seconds,speed);
    // if(!ok){
    //     print_error("transmit file");
    // }
    return 0;
}
int receive_file(int tcp_fd){
    FILE* file = fopen("testfile.txt","w");
    char buf[BUF_SIZE];
    int bytes;
    START_TIMER(timer1);
    while (true){
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
        // fwrite(buf,sizeof(char),bytes,file);
        // QueryPerformanceCounter(&end);
        // elapsed_seconds = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
        // printf("time write %.9fs\n",elapsed_seconds);
    }
    END_TIMER(timer1);
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
        shutdown(tcp_fd,SHUTDOWN_BOTH);
    }
    else if (strcmp(argv[1],"listen") == 0){
        tcp_fd = listen_to_discovery();
        receive_file(tcp_fd);
        shutdown(tcp_fd,SHUTDOWN_BOTH);
    }
    else{
        printf("invalid argument\n");
        exit(1);
    }
}