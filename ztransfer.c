#define WIN32_LEAN_AND_MEAN
#include<stdio.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
int main(int argc, char* argv[]){
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
    struct addrinfo info, *res, *p;
    memset(&info,0,sizeof info);
    info.ai_family = AF_UNSPEC;
    info.ai_socktype = SOCK_STREAM;
    info.ai_flags = AI_PASSIVE;
    int status;
    if ((status = getaddrinfo(NULL,"4000",&info,&res) !=0)){
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
            exit(1);
        }
        if (bind(sockfd,p->ai_addr, p->ai_addrlen)== -1){
            continue;
        }
        break;
    }
    freeaddrinfo(res);
    if (p == NULL){
        printf("could not bind");
        exit(1);
    }    
    if (listen(sockfd,1)==-1){
        printf("could not listen");
        exit(1);
    }
    while(1){

    }
}