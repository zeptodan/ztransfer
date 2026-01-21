#include"ztransfer-lib.h"
char buf[BUF_SIZE];
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
    Broadcast_addrs* addrs = addrs_list_constructor();
    add_all_addrs(addrs);
    int size = sizeof their_addr;
    struct timeval time;
    int new_fd;
    char* hostname = malloc(MAX_NAME);
    if (gethostname(hostname,MAX_NAME) == -1){
        print_error("host name");
        exit(1);
    }
    printf("my name is %s\n",hostname);
    int name_len = strlen(hostname);
    if (listen(tcp_fd,1)==-1){
        printf("could not listen");
        exit(1);
    }
    while(1){
        FD_ZERO(&my_set);
        FD_SET(tcp_fd,&my_set);
        time.tv_sec = 0;
        time.tv_usec = 10000;
        addrs->broadcast(addrs,udp_fd,hostname,name_len);
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
int send_metadata(char is_file,int tcp_fd,char* path){
    static int abs_len = -1;
    if(abs_len == -1){
        int i;
        int arg_len = strlen(path);
        for (i = arg_len;i > -1;i--){
            if(path[i] == SEP){
                break;
            }
        }
        if (i == -1){
            printf("invalid path %s\n",path);
            exit(1);
        }
        abs_len = i;
    }
    *buf = is_file;
    if (is_file){
        uint64_t size = get_file_size(path);
        memcpy(buf + 1,&size,sizeof(uint64_t));
    }
    uint32_t len = strlen(path)-abs_len;
    uint32_t len_network = htonl(len);
    memcpy(buf + sizeof(uint64_t) + 1,&len_network,sizeof(uint32_t));
    strcpy(buf + sizeof(uint64_t) + sizeof(uint32_t) + 1,path + abs_len);
    uint32_t size = len + sizeof(uint64_t) + sizeof(uint32_t) + 1;
    send(tcp_fd,buf,size,0);
}
Metadata receive_metadata(int tcp_fd){
    int read = 0;
    int bytes;
    while (read != METADATA){
        bytes = recv(tcp_fd,buf+read,METADATA - read,0);
        if (bytes != 0 && buf[0] == 2){
            Metadata data = {0,2,NULL};
            return data;
        }
        if (bytes <= 0){
            print_error("recv metadata");
            break;
        }
        read+=bytes;
    }
    char is_file = *buf;
    uint64_t size;
    memcpy(&size,buf + 1, sizeof(uint64_t));
    size = my_ntohll(size);
    uint32_t len;
    memcpy(&len,buf + sizeof(uint64_t) + 1,sizeof(uint32_t));
    len = ntohl(len);
    read = 0;
    while (read != len){
        read += recv(tcp_fd,buf+read,len - read,0);
    }
    char* path = malloc(len + 1);
    buf[len] = '\0';
    strcpy(path,buf);
    Metadata data = {size, is_file, path};
    return data;
}
int receive_file(int tcp_fd,uint64_t size,char* path){
    FILE* file = fopen(path,"wb");
    uint64_t tot_bytes = 0;
    int bytes;
    START_TIMER(timer1);
    while (tot_bytes !=size){
        bytes = recv(tcp_fd,buf,min(BUF_SIZE,(size - tot_bytes)),0);
        if(bytes == 0){
            break;
        }
        fwrite(buf,sizeof(char),bytes,file);
        tot_bytes += bytes;
    }
    END_TIMER(timer1);
    fclose(file);
    return 0;
}
int receive_folder(int tcp_fd,char* path){
    char* path_with_name;
    int len = strlen(path);
    while(true){
        Metadata data = receive_metadata(tcp_fd);
        if(data.is_file == 2){
            break;
        }
        path_with_name = malloc(len + strlen(data.path) + 1);
        strcpy(path_with_name,path);
        strcpy(path_with_name + len,data.path);
        if(data.is_file == 1){
            receive_file(tcp_fd,data.size,path_with_name);
        }
        else{
            create_folder(path_with_name);
        }
        free(path_with_name);
    }
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
        int isfolder = is_folder(argv[2]);
        if (isfolder == 1){
            send_folder(tcp_fd,argv[2]);
        }
        else if (isfolder == 0){
            send_file(tcp_fd,argv[2]);
        }
        char stop = 2;
        send(tcp_fd,&stop,1,0);
        shutdown(tcp_fd,SHUTDOWN_BOTH);
    }
    else if (strcmp(argv[1],"listen") == 0){
        tcp_fd = listen_to_discovery();
        receive_folder(tcp_fd,argv[2]);
        shutdown(tcp_fd,SHUTDOWN_BOTH);
    }
    else{
        printf("invalid argument\n");
        exit(1);
    }
}