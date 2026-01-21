#include"ztransfer-lib.h"
int broadcast(Broadcast_addrs* addrs,int udp_fd,char* msg,int size){
    for (int i = 0;i < addrs->size;i++){
        sendto(udp_fd,msg,size,0,(struct sockaddr*)&(addrs->addrs[i]), sizeof(struct sockaddr_in));
    }
    return 0;
}
int add_addr(Broadcast_addrs* addrs,char* addr){
    struct sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    inet_pton(AF_INET,addr,&(broadcast_addr.sin_addr));
    addrs->addrs = realloc(addrs->addrs,sizeof(struct sockaddr_in) * (addrs->size + 1));
    addrs->addrs[addrs->size++] = broadcast_addr;
    return 0;
}
int free_addrs(Broadcast_addrs* addrs){
    free(addrs->addrs);
    addrs->size = 0;
    return 0;
}
Broadcast_addrs* addrs_list_constructor(){
    Broadcast_addrs* list = malloc(sizeof(Broadcast_addrs));
    list->size = 0;
    list->addrs = NULL;
    list->add = add_addr;
    list->broadcast = broadcast;
    list->free_addrs = free_addrs;
    return list;
}