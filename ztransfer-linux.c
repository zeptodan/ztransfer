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
    struct stat st;
    stat(path,&st);
    return htobe64(st.st_size);
}
int create_folder(char* path){
    mkdir(path, 0755);
    return 0;
}
int send_file(int tcp_fd,char* path){
    send_metadata(1,tcp_fd,path);
    int fd = open(path,O_RDONLY);
    off_t offset = 0;
    struct stat st;
    stat(path,&st);
    off_t size = st.st_size;
    int n;
    while(offset < size){
        n = sendfile(tcp_fd,fd,&offset,size - offset);
        if (n <=0){
            print_error("sendfile");
            exit(1);
        }
    }
    return 0;
}
int send_folder(int tcp_fd,char* path){
    send_metadata(0,tcp_fd,path);
    DIR* d = opendir(path);
    struct dirent *e;
    struct stat st;
    char* path_with_name;
    while(e = readdir(d)){
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
        continue;
        path_with_name = malloc(strlen(path) + strlen(e->d_name) + 2);
        strcpy(path_with_name,path);
        path_with_name[strlen(path)] = SEP;
        strcpy(path_with_name + strlen(path) + 1,e->d_name);
        stat(path_with_name,&st);
        if (S_ISDIR(st.st_mode)){
            send_folder(tcp_fd,path_with_name);
        }
        else{
            send_file(tcp_fd,path_with_name);
        }
    }
    return 0;
}
int is_folder(char* path){
    struct stat st;
    stat(path,&st);
    if (S_ISDIR(st.st_mode)){
        return true;
    }
    else if(S_ISREG(st.st_mode)){
        return false;
    }
    else{
        print_error("invalid path");
        return 2;
    }
}
int add_all_addrs(Broadcast_addrs* addrs){
    struct ifaddrs* ifaddrs,*ifaddr;
    struct sockaddr_in *ip,*mask;
    char broadcast[INET_ADDRSTRLEN];
    getifaddrs(&ifaddrs);
    for(ifaddr = ifaddrs;ifaddr;ifaddr = ifaddr->ifa_next){
        if(!ifaddr->ifa_addr)
            continue;
        if(ifaddr->ifa_addr->sa_family != AF_INET)
            continue;
        ip = (struct sockaddr_in*)ifaddr->ifa_addr;
        mask = (struct sockaddr_in*)ifaddr->ifa_netmask;
        uint32_t bcast =htonl(ntohl(ip->sin_addr.s_addr) | ~ntohl(mask->sin_addr.s_addr));
        inet_ntop(AF_INET,&bcast,broadcast,sizeof(broadcast));
        addrs->add(addrs,broadcast);
    }
    return 0;
}