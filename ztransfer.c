#define WIN32_LEAN_AND_MEAN
#include<stdio.h>
#include<WinSock2.h>
#include<WS2tcpip.h>

#define MAX_BUF 100
#define PORT 4000

void windowStartup(){
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
void* get_addr(struct sockaddr* their_addr){
    if (their_addr->sa_family == AF_INET){
        return &(((struct sockaddr_in*)their_addr)->sin_addr);
    }
    else{
        return &(((struct sockaddr_in6*)their_addr)->sin6_addr);
    }
}
int get_broadcast_socket(){
    int sockfd, broadcast = 1;
    if ((sockfd = socket(AF_INET,SOCK_DGRAM,0))==-1){
        perror("socket");
        exit(1);
    }
    if(setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,(char *)&broadcast,sizeof broadcast)==-1){
        perror("socket options(broadcast)");
        exit(1);
    }
    return sockfd;
}
int broadcast(int sockfd,struct sockaddr_in* broadcast_addr){
    if (sendto(sockfd,"yo homeboy",10,0,(struct sockaddr*)broadcast_addr, sizeof(struct sockaddr_in))==-1){
        perror("sendto");
        printf("windows error: %d\n",WSAGetLastError());
        exit(1);
    }
    return 0;
}
int get_broadcast_listener(char* port){
    struct addrinfo info, *res, *p;
    memset(&info,0,sizeof info);
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_DGRAM;
    info.ai_flags = AI_PASSIVE;
    int status;
    if ((status = getaddrinfo(NULL,port,&info,&res) !=0)){
        fprintf(stderr,"gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    int sockfd;
    int yes = 1;
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
int bindtoSocket(char* port){
    struct addrinfo info, *res, *p;
    memset(&info,0,sizeof info);
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_STREAM;
    info.ai_flags = AI_PASSIVE;
    int status;
    if ((status = getaddrinfo(NULL,port,&info,&res)) !=0){
        fprintf(stderr,"gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    int sockfd;
    int yes = 1;
    for(p = res; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(const char*)&yes,sizeof(int)) == -1){
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
int connecttoSocket(char* port,char s[], int size){
    struct addrinfo info, *res, *p;
    memset(&info,0,sizeof info);
    info.ai_family = AF_UNSPEC;
    info.ai_socktype = SOCK_STREAM;
    int status;
    if ((status = getaddrinfo("localhost",port,&info,&res) !=0)){
        fprintf(stderr,"gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    int sockfd;
    for(p = res; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
            continue;
        }
        inet_ntop(p->ai_family,get_addr((struct sockaddr*)p->ai_addr),s,size);
        printf("attempting to connect to: %s\n", s);
        if (connect(sockfd,p->ai_addr,p->ai_addrlen) == -1){
            closesocket(sockfd);
            perror("error connecting");
            printf("windows error: %d\n",WSAGetLastError());
            continue;
        }
        break;
    }
    if (p == NULL){
        printf("could not bind");
        exit(1);
    }
    inet_ntop(p->ai_family,get_addr((struct sockaddr*)p->ai_addr),s,size);
    printf("connected to: %s\n", s);
    freeaddrinfo(res);
    return sockfd;
}
int discovery(){
    fd_set my_set;
    int tcp_fd = bindtoSocket("4000");

    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    printf("sockname: %i\n", getsockname(tcp_fd, (struct sockaddr*)&addr, &addrlen));
    if (getsockname(tcp_fd, (struct sockaddr*)&addr, &addrlen) == SOCKET_ERROR) {
        printf("getsockname failed: %d\n", WSAGetLastError());
    } else {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        printf("Socket is bound to IP: %s, port: %d\n", ip, ntohs(addr.sin_port));
    }
    int udp_fd = get_broadcast_socket();
    struct sockaddr_storage their_addr;
    int size = sizeof their_addr;
    struct sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    inet_pton(AF_INET,"192.168.1.255",&(broadcast_addr.sin_addr));
    if (listen(tcp_fd,1)==-1){
        printf("could not listen");
        exit(1);
    }
    struct timeval time;
    int new_fd;
    while(1){
        FD_ZERO(&my_set);
        FD_SET(tcp_fd,&my_set);
        time.tv_sec = 1;
        time.tv_usec = 0;
        int timer = select(tcp_fd + 1,&my_set,NULL, NULL,&time);
        if (timer == 0){
            broadcast(udp_fd,&broadcast_addr);
            continue;
        }
        else if(FD_ISSET(tcp_fd,&my_set)){
            if ((new_fd = accept(tcp_fd,(struct sockaddr*)&their_addr,& size)) == -1){
                perror("accept");
                continue;
            }
            break;
        }
    }
    char buf[MAX_BUF];
    int bytes = recv(new_fd,buf,MAX_BUF,0);
    if(bytes ==0){
        printf("closed\n");
    }
    if(bytes ==-1){
        printf("windows error: %d",WSAGetLastError());
    }
    printf("bytes: %i\n",bytes);

    buf[bytes] = '\0';
    printf("bytes dude: %s\n",buf);
    return new_fd;
}
int listen_to_discovery(){
    fd_set my_set;
    char buf[MAX_BUF];
    int bytes;
    int udp_fd = get_broadcast_listener("4000");
    int tcp_fd;
    struct sockaddr_storage their_addr;
    int size = sizeof their_addr;
    struct timeval time;
    while(1){
        FD_ZERO(&my_set);
        FD_SET(udp_fd,&my_set);
        time.tv_sec = 1;
        time.tv_usec = 0;
        int timer = select(udp_fd+1,&my_set,NULL,NULL,&time);
        if (timer == 0){
            continue;
        }
        else if(FD_ISSET(udp_fd,&my_set)){
            if ((bytes = recvfrom(udp_fd,buf,MAX_BUF,0,(struct sockaddr*)&their_addr,&size)) == -1){
                perror("recv");
                continue;
            }
            buf[bytes] = '\0';
            printf("bytes recv: %s\n", buf);
            break;
        }
        else{
            perror("unknown");
            printf("windows error: %d",WSAGetLastError());
            break;
        }
    }
    closesocket(udp_fd);
    struct sockaddr_in connect_addr;
    memset(&connect_addr,0,sizeof connect_addr);
    connect_addr.sin_family = ((struct sockaddr_in*)&their_addr)->sin_family;
    connect_addr.sin_addr.S_un.S_addr = INADDR_ANY;
    connect_addr.sin_port = htons(PORT);
    if ((tcp_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){
        perror("socket");
        printf("windows error: %d",WSAGetLastError());
        exit(1);
    }
    // if (bind(tcp_fd,(struct sockaddr*)&connect_addr,sizeof connect_addr) == -1){
    //     perror("bind");
    //     printf("windows error: %d",WSAGetLastError());
    //     exit(1);
    // }
    connect_addr.sin_addr = ((struct sockaddr_in*)&their_addr)->sin_addr;

    
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &connect_addr.sin_addr, ip, sizeof(ip));
    printf("Socket bound to IP: %s, port: %d\n", ip, ntohs(connect_addr.sin_port));
    
    if (connect(tcp_fd, (struct sockaddr*)&connect_addr,sizeof connect_addr) == -1){
        perror("connect");
        printf("windows error: %d",WSAGetLastError());
        exit(1);
    }
    send(tcp_fd,"hell naw",8,0);
    return 0;
}
int main(int argc, char* argv[]){
    if (argc != 2){
        printf("expected argument ./ztransfer [send | receive]\n");
    }
    windowStartup();  
    int sockfd;
    char* port = "4000";
    socklen_t size;
    char s[INET6_ADDRSTRLEN];
    if (strcmp(argv[1],"receive") == 0){
        char buf[MAX_BUF];
        int totbytes;
        sockfd = connecttoSocket(port,s,sizeof s);
        if((totbytes = recv(sockfd,buf,MAX_BUF -1,0)) == -1){
            perror("receive error");
            printf("windows error: %d",WSAGetLastError());
            exit(1);
        }
        if (totbytes == 0){
            printf("connection closed by peer\n");
        }
        buf[totbytes] = '\0';
        printf("bytes received: %s\n", buf);
        closesocket(sockfd);
    }
    else if (strcmp(argv[1],"send") == 0){
        struct sockaddr_storage their_addr;
        size = sizeof their_addr;
        sockfd = bindtoSocket(port);
        if (listen(sockfd,2)==-1){
            printf("could not listen");
            exit(1);
        }
        int new_fd;
        while(1){
            new_fd = accept(sockfd, (struct sockaddr*)&their_addr,&size);
            if (new_fd == -1){
                continue;
            }
            inet_ntop(their_addr.ss_family,get_addr((struct sockaddr*)&their_addr),s,sizeof s);
            printf("server got connection from: %s\n", s);
            if(send(new_fd,"sup homie",9,0) == -1){
                perror("error sending message");
            }
            closesocket(new_fd);
        }
    }
    else if (strcmp(argv[1],"broadcast") == 0){
        discovery();
    }
    else if (strcmp(argv[1],"listen") == 0){
        listen_to_discovery();
    }
    else{
        printf("invalid argument\n");
        exit(1);
    }
}