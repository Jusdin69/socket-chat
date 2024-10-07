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
// struct sockaddr_in {
//     short            sin_family;   // AF_INET,因為這是IPv4;
//     unsigned short   sin_port;     // 儲存port No
//     struct in_addr   sin_addr;     // 參見struct in_addr
//     char             sin_zero[8];  // Not used, must be zero */
// };

// struct in_addr {
//     unsigned long s_addr;          // load with inet_pton()
// };

const char* host = "0.0.0.0";
int port = 7000;
const char* echo = ": ";
struct client_node;
std::vector<client_node> client_list;
std::mutex broadcast_lock, client_lock;
struct client_node{
    int socket;
    char name[128];
    client_node(int fd, const char* s){
        socket = fd;
        strcpy(name, s);
    }
};

void insert_client(int sock_fd, const char* s){
    std::lock_guard<std::mutex> lock(client_lock);
    client_list.emplace_back(sock_fd, s);
}

void private_message(int client_socket, const char *inmsg, const char* my_name , char *outmsg){
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
void broaddcast(char* outmsg, int sender){
    //std::lock_guard<std::mutex> lock(broadcast_lock);
    for(auto &client:client_list){
        if(client.socket != sender) {
            send(client.socket, outmsg, strlen(outmsg), 0);
        }
    }
}
void handler(int client_socket){
    char inmsg[1024] = {0};
    char outmsg[1152] = {0};
    char name[128] = {0};
    static int connection_count = 0;
    connection_count +=1;
    // receive new client name
    printf("Number of connected: %d\n", connection_count);
    int nbytes = recv(client_socket, name, sizeof(name), 0);
    insert_client(client_socket, name);
    printf("%s jointed\n", name);
    sprintf(outmsg, "Number of connected: %d\n%s jointed", connection_count,name);
    broaddcast(outmsg, client_socket);
    while (1) {
        memset(inmsg,0 , sizeof(inmsg));
        memset(outmsg,0 , sizeof(outmsg));
        int nbytes = recv(client_socket, inmsg, sizeof(inmsg), 0);
        if (nbytes <= 0) {
            close(client_socket);
            printf("client closed connection.\n");
            std::lock_guard<std::mutex> lock(client_lock);
            // remove client node from list    
            client_list.erase(
                std::remove_if(client_list.begin(), client_list.end(), [&](const client_node &c){return c.socket==client_socket;})
                ,client_list.end());
            break;
               
        }
        if(nbytes >0 && inmsg[0] == '@') {
            private_message(client_socket,inmsg, name, outmsg);
        }
        else{
            sprintf(outmsg, "%s%s%s",name,echo, inmsg);
            broaddcast(outmsg, client_socket);
        }
    }
    memset(outmsg,0 , sizeof(outmsg));
    connection_count -=1;
    sprintf(outmsg, "Number of connected: %d\n%s left", connection_count,name);
    broaddcast(outmsg, client_socket);
}

int main()
{
    int sock_fd;
    struct sockaddr_in my_addr;
    socklen_t addlen;
    int status;
    char indata[1024] = {0}, outdata[1024] = {0};
    int on = 1;
    // create a socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation error");
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

    if (bind(sock_fd, (struct sockaddr *)&my_addr, sizeof(my_addr)); == -1) {
        perror("Binding error");
        exit(1);
    }
    printf("server start at: %s:%d\n", inet_ntoa(my_addr.sin_addr), port);

    if (listen(sock_fd, 5);) {
        perror("Listening error");
        exit(1);
    }
    printf("wait for connection...\n");
        
    while (1) {
        struct sockaddr_in client_addr;
        addlen = sizeof(client_addr);
        int new_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addlen);
        printf("connected by %s:%d\n", inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port));
        std::thread client_thread(handler, new_fd);
        client_thread.detach();
    }
    close(sock_fd);
    return 0;
}