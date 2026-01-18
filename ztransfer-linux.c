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
uint64_t get_file_size(char* path){
    return be64toh();
}
int create_folder(char* path){
    return 0;
}
int send_file(int tcp_fd,char* path){
    send_metadata(1,tcp_fd,path);
}
int send_folder(int tcp_fd,char* path){
    send_metadata(0,tcp_fd,path);
}