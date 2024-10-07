#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<thread>

const char* host = "0.0.0.0";
int port = 7000;
void receiving_handler(int sock_fd){
    char buf[1024];
    while(1){
        memset(buf, 0 , sizeof(buf));
        int nbytes = recv(sock_fd, buf, sizeof(buf), 0);
        if (nbytes <= 0) {
            close(sock_fd);
            printf("server disconnected\n");
            break;
        }
        printf("%s\n", buf);
    }
}
int main()
{
    int sock_fd;
    struct sockaddr_in serv_name;
    int status;
    char indata[1152] = {0};
    char outdata[1024] = {0};
    char name[128] = {0};
    // create a socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation error");
        exit(1);
    }
    // server address
    serv_name.sin_family = AF_INET;
    inet_aton(host, &serv_name.sin_addr);
    serv_name.sin_port = htons(port);
    status = connect(sock_fd, (struct sockaddr *)&serv_name, sizeof(serv_name));
    if (status == -1) {
        perror("Connection error");
        exit(1);
    }

    printf("Enter user name:\n");
    fgets(name, sizeof(name), stdin);
    send(sock_fd, name, strlen(name)-1, 0);
    std::thread receive_thread(receiving_handler, sock_fd);
    receive_thread.detach();
    while (1) {
        memset(outdata,0 , sizeof(outdata));
        fgets(outdata, sizeof(outdata), stdin);
        printf("Me: %s", outdata);
        send(sock_fd, outdata, strlen(outdata)-1, 0);
        
    }

    return 0;
}