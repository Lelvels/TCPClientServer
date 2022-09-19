#include<iostream>
#include<sys/types.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<string.h>
#include<string>
#include<fcntl.h>
#include<sys/epoll.h>

#include"common.h"

#define RECVBUFSIZE 100
#define MAX_EVENTS 2

using json = nlohmann::json;
static int msg_id = 0;

void connectToServer(int clientSock, int serverPort){
    std::string ipAddress = "127.0.0.1";
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(serverPort);
    inet_pton(AF_INET, ipAddress.c_str(), &hint.sin_addr);

    int connectResult = connect(clientSock, (sockaddr *) &hint, sizeof(hint));
    if(connectResult < 0){
        std::cerr << "[+] Connect to server failed!" << std::endl;
        exit(1);
    } else {
        std::cout << "[+] Connect to server successfully!" << "\n";
    }
}

std::string generate_message(json &j, char* buffer, int buffer_size){
    message::request req;
    std::map<std::string, std::string> my_param;
    std::string msg(buffer);
    my_param.insert(std::pair<std::string, std::string>("message", msg));
    req.device_name = "esp1";
    req.dest = "server";
    req.params = my_param;
    req.req_method = "send";
    req.id = msg_id;
    message::serializeRequest(j, req);
    msg_id ++;
    return j.dump();
}

int main(int argc, char *argv[]){
    int clientSock, echoServerPort;

    /* Setup connection */
    clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(clientSock < 0){
        std::cerr << "[+] Cannot create client sock!" << "\n";
        exit(1);
    }

    if(argc != 2){
        std::cerr << "Usage client port: " << argv[0] << "\n";
        exit(1);
    }

    echoServerPort = atoi(argv[1]);
    connectToServer(clientSock, echoServerPort);

    /* Starting */
    int epfd, event_count, fd;
    size_t bytes_read;
    struct epoll_event ev;
    struct epoll_event evlist[MAX_EVENTS];
    char buffer[RECVBUFSIZE+1];
    int running = 1;

    epfd = epoll_create(MAX_EVENTS);
    if(epfd == -1){
        std::cerr << "Epoll create failed!" << std::endl;
    }

    /* Add epoll event receiving from server */
    ev.events = EPOLLIN;
    ev.data.fd = clientSock;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, clientSock, &ev) == -1){
        std::cerr << "[+] epoll_ctl recving from server failed" << "\n";
        close(epfd);
        exit(EXIT_FAILURE); 
    }
    /* Add epoll event input from user keyboard */
    ev.events = EPOLLIN;
    ev.data.fd = 0;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &ev) == -1){
        std::cerr << "[+] epoll_ctl waiting for user input failed" << "\n";
        close(epfd);
        exit(EXIT_FAILURE); 
    }
    do{
        printf("About to epoll_wait(), type exit to close client\n");
        event_count = epoll_wait(epfd, evlist, MAX_EVENTS, -1);
        if(event_count == -1){
            if(errno == EINTR)
                continue;
            else
                std::cerr << "epoll_wait!" << std::endl;
        }
        printf("[+] Event count: %d\n", event_count);
        for(int i=0; i<event_count; i++){
            printf("[+] Reading file descriptor '%d' with ", evlist[i].data.fd);
            if(evlist[i].data.fd == 0){
                bytes_read = read(evlist[i].data.fd, buffer, sizeof(buffer));
                printf("%zd bytes read.\n", bytes_read);
                buffer[bytes_read] = '\0';
                printf("[+] Read %s\n", buffer);
                json j;
                std::string message = generate_message(j, buffer, bytes_read);
                if(send(clientSock, message.c_str(), message.size(), 0) == -1){
                    printf("[+] Message sent failed!");
                }
                if(!strncmp(buffer, "exit\n", 5)) 
                    running = 0;
            } else if(evlist[i].data.fd == clientSock){
                bytes_read = recv(clientSock, buffer, RECVBUFSIZE, 0);
                if(bytes_read == -1){
                    std::cout << "[+] There was a error getting a response from server!\n";
                } else {
                    std::cout << "[+] SERVER > " << std::string(buffer, bytes_read) << "\r\n";
                }
            }
        }
    } while(running);

    close(clientSock);
    exit(EXIT_SUCCESS);
}