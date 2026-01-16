#include"ztransfer-lib.h"
void print_error(char* msg){
    printf("%s",msg);
    perror("");
}
void window_startup(){
    return;
}
int close_socket(int socket){
    close(socket);
    return 0;
}