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
    int size = 1 << 23;
    for(p = res; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(const char*)&yes,sizeof(int)) == -1){
            print_error("error in sockopt");
            close_socket(sockfd);
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF,(const char*)&size,sizeof(int)) == -1){
            print_error("error in sockopt");
            close_socket(sockfd);
            continue;
        }
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,(const char*)&yes,sizeof(int)) == -1){
            print_error("error in sockopt");
            close_socket(sockfd);
            continue;
        }
        if (bind(sockfd,p->ai_addr, p->ai_addrlen)== -1){
            print_error("could not bind");
            close_socket(sockfd);
            continue;
        }
        break;
    }
    if (p == NULL){
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
    int size = 1 << 23;
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
int draw_select(int option,int linenum,int count, char* strings[]){
    int x = getmaxx(stdscr);
    attrset(A_NORMAL);
    for (int i = 0; i < count; i++){
        attrset(option == i? COLOR_PAIR(1): A_NORMAL);
        mvprintw(linenum++,0,"%-*s",x,strings[i]);
    }
    attrset(A_NORMAL);
    refresh();
}
int make_choice(int linenum,int time,int count,char* strings[]){
    timeout(time);
    int option = 0;
    bool loop = true;
    draw_select(option,linenum,count,strings);
    while(loop){
        int ch = getch();
        switch(ch){
            case KEY_UP:
                option = (--option + count) % count;
                draw_select(option,linenum,count,strings);
                break;
            case KEY_DOWN:
                option = (++option + count) % count;
                draw_select(option,linenum,count,strings);
                break;
            case KEY_RESIZE:
                resize_term(0,0);
                clear();
                draw_select(option,linenum,count,strings);
                break;
            case ERR:
                return ERR;
            case KEY_ENTER:
            case 10:
                loop = false;
                break;
        }
    }
    return option;
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
    int name_len = strlen(hostname);
    if (listen(tcp_fd,1)==-1){
        printf("could not listen");
        exit(1);
    }
    timeout(100);
    int x = getmaxx(stdscr);
    clear();
    mvprintw(0,0,"ztransfer");
    mvprintw(1,0,"Your name: %s",hostname);
    mvprintw(2,0,"%s","Broadcasting...");
    attron(COLOR_PAIR(1));
    mvprintw(4,0,"%-*s",x,"Cancel");
    refresh();
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
            int bytes = 0;
            while (bytes != sizeof(int)){
                bytes += recv(new_fd,buf,sizeof(int) - bytes,0);
            }
            int len;
            memcpy(&len,buf,sizeof(int));
            len = ntohl(len);
            bytes = 0;
            while (bytes != len){
                bytes += recv(new_fd,buf,len - bytes,0);
            }
            buf[len] = '\0';
            attroff(COLOR_PAIR(1));
            mvprintw(5,0,"%s is trying to connect",buf);
            mvprintw(4,0,"Cancel");
            clrtoeol();
            char* strings[2] = {"accept","reject"};
            int option = make_choice(6,-1,2,strings);
            timeout(100);
            if(option == 0){
                int yes = htonl(1);
                send(new_fd,(char*)&yes,sizeof(int),0);
                break;
            }
            else{
                int no = htonl(0);
                send(new_fd,(char*)&no,sizeof(int),0);
                shutdown(new_fd,SHUTDOWN_BOTH);
                close_socket(new_fd);
                clear();
                mvprintw(0,0,"%-*s",x,"ztransfer");
                mvprintw(2,0,"%-*s",x,"Broadcasting...");
                attron(COLOR_PAIR(1));
                mvprintw(4,0,"%-*s",x,"Cancel");
                refresh();
            }
        }
        int ch = getch();
        if(ch == KEY_ENTER || ch == 10){
            exit(1);
        }
    }
    close_socket(udp_fd);
    close_socket(tcp_fd);
    return new_fd;
}
int draw_broadcasts(int option, BroadcastList* list){
    int x = getmaxx(stdscr);
    clear();
    attrset(A_NORMAL);
    mvprintw(0,0,"%-*s",x,"ztransfer");
    mvprintw(2,0,"%-*s",x,"Listening...");
    if (option == 0){
        attron(COLOR_PAIR(1));
    }
    mvprintw(4,0,"%-*s",x,"Cancel");
    for (int i = 0; i < list->size; i++){
        attrset(option - 1 == i? COLOR_PAIR(1): A_NORMAL);
        mvprintw(6 + i,0,"%-*s",x,list->broadcasts[i].name);
    }
    refresh();
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
    timeout(100);
    int option = 0;
    draw_broadcasts(option,list);
    bool loop = true;
    while(loop){
        FD_ZERO(&my_set);
        FD_SET(udp_fd,&my_set);
        time.tv_sec = 0;
        time.tv_usec = 10000;
        select(udp_fd+1,&my_set,NULL,NULL,&time);
        if(FD_ISSET(udp_fd,&my_set)){
            if ((bytes = recvfrom(udp_fd,buf,MAX_BUF,0,(struct sockaddr*)&their_addr,&size)) == -1){
                continue;
            }
            buf[bytes] = '\0';
            char* name = malloc(bytes+1);
            strcpy(name,buf);
            list->add(list,their_addr,name);
        }
        list->clean(list);
        if(option - 1 >= list->size){
            option = list->size;
        }
        draw_broadcasts(option,list);
        int ch = getch();
        switch(ch){
            case KEY_UP:
                option = (--option + (list->size + 1)) % (list->size + 1);
                draw_broadcasts(option,list);
                break;
            case KEY_DOWN:
                option = (++option + (list->size + 1)) % (list->size + 1);
                draw_broadcasts(option,list);
                break;
            case KEY_RESIZE:
                resize_term(0,0);
                clear();
                draw_broadcasts(option,list);
                break;
            case KEY_ENTER:
            case 10:
                loop = false;
                break;
        }
    }
    close_socket(udp_fd);
    if(option == 0){
        exit(1);
    }
    tcp_fd = get_tcp_socket(list->broadcasts[option - 1].addr);
    attroff(COLOR_PAIR(1));
    mvprintw(5,0,"waiting for response...");
    refresh();
    char* hostname = malloc(MAX_NAME);
    if (gethostname(hostname,MAX_NAME) == -1){
        print_error("host name");
        exit(1);
    }
    int name_len = strlen(hostname);
    int net_len = htonl(name_len);
    if(send(tcp_fd,(char*)&net_len,sizeof(int),0) == -1){
        print_error("send");
        exit(1);
    }

    if(send(tcp_fd,hostname,name_len,0) == -1){
        print_error("send");
        exit(1);
    }
    if(recv(tcp_fd,(char*)&name_len,sizeof(int),0) == -1){
        print_error("recv");
        exit(1);
    }
    name_len = ntohl(name_len);
    if(name_len == 0){
        printf("connection rejected\n");
        exit(1);
    }
    list->list_free(list);
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
    for (int i = 0; i < len; i++){
        if (path[i] == NOT_SEP){
            path[i] = SEP;
        }
    }
    Metadata data = {size, is_file, path};
    return data;
}
int receive_file(int tcp_fd,uint64_t size,char* path){
    static int linenum = 4;
    int y = getmaxy(stdscr);
    FILE* file = fopen(path,"wb");
    uint64_t tot_bytes = 0;
    int bytes;
    int percent;
    time_t tim = time(NULL);
    time_t next;
    START_TIMER(timer1);
    while (tot_bytes !=size){
        printf("recv\n");
        START_TIMER(timer3);
        bytes = recv(tcp_fd,buf,min(BUF_SIZE,(size - tot_bytes)),0);
        END_TIMER(timer3);
        printf("bytes: %f KB\n",bytes/1024.0);
        if(bytes == 0){
            break;
        }
        printf("write\n");
        START_TIMER(timer4);
        fwrite(buf,sizeof(char),bytes,file);
        END_TIMER(timer4);
        tot_bytes += bytes;
        START_TIMER(timer2);
        next = time(NULL);
        if(next - tim > 2 || tot_bytes == size){
            tim = next;
            mvprintw(linenum,0,"%s",path);
            percent = (tot_bytes / (float)size) *100;
            printw(" (%d%%)",percent);
            percent = (percent / 100.0) * 30;
            move(linenum+1, 0);
            clrtoeol();
            mvprintw(linenum+1, 0, "[%.*s%.*s]", percent, "##############################", 30-percent, "------------------------------");
            refresh();
        }
        END_TIMER(timer2);
    }
    if(linenum + 3 >= y){
        scroll(stdscr);
        scroll(stdscr);
        linenum = y - 2;
    }
    else{
        linenum += 2;
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
void cleanup_ncurses(void){
    endwin();
}
int main(int argc, char* argv[]){
    initscr();
    scrollok(stdscr, TRUE);
    cbreak();
    noecho();
    nl();
    curs_set(0);
    keypad(stdscr,true);
    start_color();
    atexit(cleanup_ncurses);
    mvprintw(0,0,"ztransfer");
    init_pair(1,COLOR_WHITE,COLOR_BLUE);
    char* strings[3] = {"Broadcast","Listen","Cancel"};
    int option = make_choice(3,-1,3,strings);
    if(option==2){
        return 0;
    }
    window_startup();
    int tcp_fd;
    clear();
    mvprintw(0,0,"ztransfer");
    mvprintw(2,0,"File path: ");
    echo();
    char pathbuf[1024];
    getnstr(pathbuf,sizeof(pathbuf) - 1);
    int isfolder = is_folder(pathbuf);
    if (isfolder == 2){
        exit(1);
    }
    switch(option){
        case 0:
            tcp_fd = discovery();
            clear();
            mvprintw(0,0,"ztransfer");
            mvprintw(2,0,"Sending...");
            refresh();
            if (isfolder == 1){
                send_folder(tcp_fd,pathbuf);
            }
            else if (isfolder == 0){
                send_file(tcp_fd,pathbuf);
            }
            char stop = 2;
            send(tcp_fd,&stop,1,0);
            shutdown(tcp_fd,SHUTDOWN_BOTH);
            break;
        case 1:
            tcp_fd = listen_to_discovery();
            clear();
            mvprintw(0,0,"ztransfer");
            mvprintw(2,0,"receiving...");
            refresh();
            receive_folder(tcp_fd,pathbuf);
            shutdown(tcp_fd,SHUTDOWN_BOTH);
            break;
    }
}