#include"ztransfer-lib.h"
void print_error(char* msg){
    printf("%s: %d\n",msg,WSAGetLastError());
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
int close_socket(int socket){
    closesocket(socket);
    return 0;
}