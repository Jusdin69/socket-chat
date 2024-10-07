#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<thread>
#include<mutex>
#include<vector>
#include <algorithm>
#include <sys/epoll.h>
#include <set>

#define OPEN_MAX 1024
const char* host = "0.0.0.0";
const char* invalid_name = "invalid name! try again";
int port = 7000;
struct client_node;
static std::vector<client_node> client_list;
static std::set<int> client_fd_set;
static std::mutex client_lock;
static void private_message(int client_socket, const char *inmsg, const char* my_name , char *outmsg);
static void instruction(const char* inmsg, int sender, char *outmsg);
struct client_node{
    int socket;
    char name[128];
    client_node(int fd, const char* s){
        socket = fd;
        strcpy(name, s);
    }
};

// send message to every one except for the sender
// if sender ==-1 send to everyone
static void broaddcast(char* outmsg, int sender){
    if(sender == -1){
        for(auto &client:client_list){
            send(client.socket, outmsg, strlen(outmsg), 0);
        }
    }
    else{
        for(auto &client:client_list){
            if(client.socket != sender) {
                send(client.socket, outmsg, strlen(outmsg), 0);
            }
        }
    }
}
// insert the client name and the corrsponding fd to client list
static void insert_client(int sock_fd, const char* name){
    std::lock_guard<std::mutex> lock(client_lock);
    client_list.emplace_back(sock_fd, name);
    client_fd_set.insert(sock_fd);
}
// remove the disconnect client from client list
static void remove_client(int client_socket, int epoll_fd, epoll_event* listen_fd, char *outmsg){
    char name[128] = {0};
    //char outmsg[1152] = {0};
    for(auto &client:client_list){
        if(client.socket == client_socket) {
            strncpy(name, client.name, sizeof(name));
        }
    }
    std::lock_guard<std::mutex> lock(client_lock); 
    client_list.erase(
        std::remove_if(client_list.begin(), client_list.end(), [&](const client_node &c){return c.socket==client_socket;})
        ,client_list.end());
    client_fd_set.erase(client_socket);
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, listen_fd) == -1) {
        perror("epoll_ctl failed");
    }
    sprintf(outmsg, "%s left\nNumber of connected: %ld", name, client_list.size());
    
}
//handing new message the send to target client
static void new_message(const char* inmsg, int sender, char *outmsg){
    char name[128] = {0};
    //char outmsg[1152] = {0};
    for(auto &client:client_list){
        if(client.socket == sender) {
            strncpy(name, client.name, sizeof(name));
        }
    }
    if(inmsg[0] == '@'){
        private_message(sender,inmsg, name, outmsg);
    }
    else if(inmsg[0] == '-'){
        instruction(inmsg, sender, outmsg);
    }
    else{
        sprintf(outmsg, "%s: %s", name, inmsg);
        broaddcast(outmsg, sender);
    }
}
static bool name_validity(const char* name){
    int len = strlen(name);
    for(int i = 0 ; i < len; i++){
        if(name[i] == ' ') return false;
    }
    for(const auto &node: client_list){
        if(strcmp(node.name,name)==0){
            return false;
        }
    }
    return true;
}
static bool user_exist(int client_fd){
    if(client_fd_set.find(client_fd) != client_fd_set.end()) return true;
    else return false;
    // for(const auto &node: client_list){
    //     if(node.socket == client_fd){
    //         return true;
    //     }
    // }
    // return false;
}
//if it's a pm, search the client list to find the fd
static void private_message(int client_socket, const char *inmsg, const char* my_name , char *outmsg){
    char target_name[128] = {0};
    if(strlen(inmsg)>1) 
        sscanf((char*)(inmsg)+1, "%s", target_name);
    size_t msg_len = strlen(inmsg) > strlen(target_name)+1 ? strlen(target_name)+2 :0;
    int target_scoket = -1;
    for(const auto &node: client_list){
        if(strcmp(node.name,target_name)==0){
            target_scoket = node.socket;
            if(msg_len > 0){
                sprintf(outmsg, "PM from %s: %s",my_name, (char*)(inmsg + msg_len));
                send(target_scoket, outmsg, strlen(outmsg), 0);
            }
            break;
        }
    }
    if(target_scoket == -1){
        sprintf(outmsg, "%s not found^^", target_name);
        send(client_socket, outmsg, strlen(outmsg), 0);
    }
}
//handling new connection, waiting the name of the client(first message from client)
static void new_connection(int client_socket, const char *name){
    // char inmsg[1024] = {0};
    // char name[128] = {0};
    char outbuf[1152] = {0};
    // receive new client name
    bool name_valid = name_validity(name);
    if(!name_valid){
        send(client_socket, invalid_name, strlen(invalid_name), 0);
    }
    else{
        insert_client(client_socket, name);
        printf("%s jointed\n", name);
        sprintf(outbuf, "%s jointed\nNumber of connected: %ld", name, client_list.size());
        broaddcast(outbuf, -1);
    }
    
}
static void instruction(const char* inmsg, int sender, char *outmsg){
    size_t pos = 0;
    size_t buf_size = sizeof(outmsg);
    if(strcmp(inmsg, "-user") == 0){
        for(const auto &node: client_list){
            if(pos + strlen(node.name) + 1 >= buf_size){
                printf("buffer overflow\n");
                break;
            }
            memcpy(outmsg + pos, node.name, strlen(node.name));
            pos += strlen(node.name);
            outmsg[pos] = ' ';
            pos++;
        }
        outmsg[pos-1] = 0;
        send(sender, outmsg, sizeof(outmsg), 0);
    }
}


int main()
{   
    
    int sock_fd;
    struct sockaddr_in my_addr;
    socklen_t addlen;
    int status;
    char indata[1024] = {0}, outbuf[1152] = {0};
    int on = 1;
    //epoll
    struct sockaddr_in address;
    struct epoll_event listen_fd, events[OPEN_MAX];
    // create a socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation error");
        exit(1);
    }

    int epoll_fd = epoll_create(OPEN_MAX);
    if(epoll_fd == -1){
        perror("epoll_create error");
        exit(1);
    }

    // for "Address already in use" error message
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) {
        perror("Setsockopt error");
        exit(1);
    }
    // server address
    my_addr.sin_family = AF_INET;
    inet_aton(host, &my_addr.sin_addr);
    my_addr.sin_port = htons(port);

    if (bind(sock_fd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
        perror("Binding error");
        exit(1);
    }
    printf("server start at: %s:%d\n", inet_ntoa(my_addr.sin_addr), port);

    if (listen(sock_fd, 5)==-1) {
        perror("Listening error");
        exit(1);
    }
    listen_fd.events = EPOLLIN;
    listen_fd.data.fd = sock_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &listen_fd) == -1) {
        perror("epoll_ctl");
        close(sock_fd);
        exit(1);
    }
    printf("wait for connection...\n");
    
    while (1) {
        memset(indata,0 , sizeof(indata));
        memset(outbuf,0 , sizeof(outbuf));
        int nfds = epoll_wait(epoll_fd, events, OPEN_MAX,-1);
        if(nfds == -1){
            perror("epoll_wait failed");
            break;
        }
        for(int i = 0; i < nfds; i ++){
            if(events[i].data.fd == sock_fd){
                struct sockaddr_in client_addr;
                addlen = sizeof(client_addr);
                int new_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addlen);
                // std:: thread handler(new_connection,new_fd, epoll_fd, &listen_fd);
                // handler.detach();
                listen_fd.events = EPOLLIN;
                listen_fd.data.fd = new_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &listen_fd) == -1) {
                    perror("epoll_ctl failed");
                    close(sock_fd);
                }
                else printf("connected by %s:%d\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
            }
            else{
                int client_fd = events[i].data.fd;
                int nbytes = read(client_fd, indata, sizeof(indata));
                if (nbytes <= 0) {
                    remove_client(client_fd, epoll_fd, &listen_fd, outbuf);
                    broaddcast(outbuf, -1);
                    printf("fd: %d Disconnected.\n", client_fd);
                    close(client_fd);
                }
                else{
                    if(user_exist(client_fd)){
                        new_message(indata, client_fd, outbuf);
                    }
                    else{
                        new_connection(client_fd, indata);
                    }
                    
                } 
            }
        }
    }
    close(sock_fd);
    return 0;
}