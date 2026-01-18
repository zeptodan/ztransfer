#include"ztransfer-lib.h"
bool is_old(Broadcast* self){
    return time(NULL) - self->time > TIMEOUT;
}
bool check_broadcast_same(struct sockaddr_storage* addr1,struct sockaddr_storage* addr2){
    #ifdef _WIN32
    return memcmp((&((struct sockaddr_in*)addr1)->sin_addr.S_un.S_addr),&(((struct sockaddr_in*)addr2)->sin_addr.S_un.S_addr), sizeof(((struct sockaddr_in*)addr2)->sin_addr.S_un.S_addr));
    #else
    return memcmp((&((struct sockaddr_in*)addr1)->sin_addr.s_addr),&(((struct sockaddr_in*)addr2)->sin_addr.s_addr), sizeof(((struct sockaddr_in*)addr2)->sin_addr.s_addr));
    #endif
}
int add(BroadcastList* self,struct sockaddr_storage addr, char* name){
    for (int i = 0;i < self->size;i++){
        if(check_broadcast_same(self->broadcasts[i].addr,&addr) == 0){
            self->broadcasts[i].time = time(NULL);
            return 0;
        }
    }
    if (self->size < MAX_PEERS){
        struct sockaddr_storage* addr1 = malloc(sizeof(struct sockaddr_storage));
        memcpy(addr1,&addr,sizeof(struct sockaddr_storage));
        Broadcast broadcast = {addr1,name,time(NULL),is_old};
        self->broadcasts[self->size++] = broadcast;
        return 0;
    }
    return -1;
}
int clean(BroadcastList* self){
    for (int i = 0;i < self->size;i++){
        if (self->broadcasts[i].is_old(&self->broadcasts[i])){
            free(self->broadcasts[i].name);
            free(self->broadcasts[i].addr);
            memmove(&self->broadcasts[i],&self->broadcasts[i+1],self->size - i - 1);
            self->size--;
        }
    }
    return 0;
}
int list_free(BroadcastList* self){
    for (int i = 0;i < self->size;i++){
        free(self->broadcasts[i].name);
        free(self->broadcasts[i].addr);
    }
    free(self);
    return 0;
}
BroadcastList* list_constructor(){
    BroadcastList* list = malloc(sizeof(BroadcastList));
    list->size = 0;
    list->clean = clean;
    list->list_free = list_free;
    list->add = add;
    return list;
}