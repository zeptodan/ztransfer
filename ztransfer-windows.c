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
uint64_t get_file_size(char* path){
    WIN32_FILE_ATTRIBUTE_DATA file_data;
    GetFileAttributesExA(path, GetFileExInfoStandard, &file_data);
    return htonll(((uint64_t)file_data.nFileSizeHigh << 32) | file_data.nFileSizeLow);
}
int create_folder(char* path){
    CreateDirectoryA(path,NULL);
    return 0;
}
int send_file(int tcp_fd,char* path){
    send_metadata(1,tcp_fd,path);
    u_long mode = 1;
    PFN_TRANSMITFILE pTransmitFile;
    pTransmitFile = (PFN_TRANSMITFILE)GetProcAddress(
        GetModuleHandleW(L"mswsock.dll"), 
        "TransmitFile"
    );
    HANDLE file = CreateFile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file == INVALID_HANDLE_VALUE) {
        printf("CreateFile failed: %lu\n", GetLastError());
        return 0;
    }
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    START_TIMER(timer1);
    bool ok = pTransmitFile(tcp_fd, file, 0, 0, &ov, NULL, 0);
    WaitForSingleObject(ov.hEvent, INFINITE);
    if(!ok && WSAGetLastError() != ERROR_IO_PENDING){
        print_error("transmit file");
        exit(1);
    }
    DWORD ignored;
    GetOverlappedResult((HANDLE)tcp_fd, &ov, &ignored, FALSE);
    END_TIMER(timer1);
    return 0;
}
int send_folder(int tcp_fd,char* path){
    send_metadata(0,tcp_fd,path);
    char* search = malloc(strlen(path) + 4);
    snprintf(search,strlen(path)+ 4,"%s\\*",path);
    WIN32_FIND_DATAA f;
    HANDLE h = FindFirstFileA(search,&f);
    char* path_with_name;
    do {
        if (!strcmp(f.cFileName, ".") || !strcmp(f.cFileName, ".."))
            continue;
        if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            continue;
        }
        path_with_name = malloc(strlen(path) + strlen(f.cFileName) + 2);
        strcpy(path_with_name,path);
        path_with_name[strlen(path)] = SEP;
        strcpy(path_with_name + strlen(path) + 1,f.cFileName);
        if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            send_folder(tcp_fd,path_with_name);
        }
        else{
            send_file(tcp_fd,path_with_name);
        }
        free(path_with_name);
    }while(FindNextFileA(h,&f));
    return 0;
}
bool is_folder(char* path){
    DWORD attr = GetFileAttributesA(path);
        if (attr == INVALID_FILE_ATTRIBUTES){
            print_error("invalid path");
            exit(1);
        }
        if (attr & FILE_ATTRIBUTE_DIRECTORY){
            return true;
        }
        else{
            return false;
        }
}