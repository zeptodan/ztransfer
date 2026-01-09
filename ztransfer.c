#define WIN32_LEAN_AND_MEAN
#include<stdio.h>
#include<WinSock2.h>
#include<WS2tcpip.h>

#define MAX_BUF 100
#define PORT 4000
#define STR(x) #x
#define XSTR(x) STR(x)
#define PORT_STRING XSTR(PORT)
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
int get_tcp_socket(struct sockaddr_storage* their_addr){
    int tcp_fd;
    struct sockaddr_in connect_addr;
    memset(&connect_addr,0,sizeof connect_addr);
    connect_addr.sin_family = ((struct sockaddr_in*)&their_addr)->sin_family;
    connect_addr.sin_addr = ((struct sockaddr_in*)&their_addr)->sin_addr;    
    connect_addr.sin_port = htons(PORT);
    if ((tcp_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){
        print_error("socket");
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
    inet_pton(AF_INET,"192.168.1.255",&(broadcast_addr.sin_addr));
    if (listen(tcp_fd,1)==-1){
        printf("could not listen");
        exit(1);
    }
    while(1){
        FD_ZERO(&my_set);
        FD_SET(tcp_fd,&my_set);
        time.tv_sec = 1;
        time.tv_usec = 0;
        int timer = select(tcp_fd + 1,&my_set,NULL, NULL,&time);
        if (timer == 0){
            send_udp_packet(udp_fd,&broadcast_addr,"yo homeboy",10);
            continue;
        }
        else if(FD_ISSET(tcp_fd,&my_set)){
            if ((new_fd = accept(tcp_fd,(struct sockaddr*)&their_addr,& size)) == -1){
                print_error("accept");
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
    int udp_fd = get_udp_listener();
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
                print_error("recv");
                continue;
            }
            buf[bytes] = '\0';
            printf("bytes recv: %s\n", buf);
            break;
        }
        else{
            print_error("unknown");
            break;
        }
    }
    closesocket(udp_fd);
    int tcp_fd = get_tcp_socket(&their_addr);
    if (send(tcp_fd,"hell naw",8,0) == -1){
        print_error("send");
    }
    return 0;
}
int main(int argc, char* argv[]){
    if (argc != 2){
        printf("expected argument ./ztransfer [send | receive]\n");
    }
    window_startup();
    if (strcmp(argv[1],"broadcast") == 0){
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