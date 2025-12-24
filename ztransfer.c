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
    struct addrinfo info;
    struct addrinfo* res;
    memset(&info,0,sizeof info);
    info.ai_family = AF_UNSPEC;
    info.ai_socktype = SOCK_STREAM;
    info.ai_flags = AI_PASSIVE;
    int status;
    if ((status = getaddrinfo(NULL,"4000",&info,&res) !=0)){
        fprintf(stderr,"gai error: %s\n", gai_strerror(status));
        exit(1);
    }
    freeaddrinfo(res);
}