#define WIN32_LEAN_AND_MEAN
#include<stdio.h>
#include<WinSock2.h>
#include<WS2tcpip.h>

#define MAX_BUF 100

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
int bindtoSocket(char* port){
    struct addrinfo info, *res, *p;
    memset(&info,0,sizeof info);
    info.ai_family = AF_UNSPEC;
    info.ai_socktype = SOCK_STREAM;
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
    else{
        printf("invalid argument\n");
        exit(1);
    }
    

}