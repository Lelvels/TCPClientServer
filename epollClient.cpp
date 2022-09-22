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

#define RECVBUFSIZE 1024
#define MAX_EVENTS 2

using json = nlohmann::json;
static int msg_id = 0;
static std::string device_name("\0");
static bool registered = false;

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

message::request create_request(std::string destination, 
                                std::string method,
                                std::map<std::string, std::string> params){
    message::request req;
    req.device_name = device_name;
    req.dest = destination;
    req.req_method = method;
    req.params = params;
    req.id = msg_id;
    return req;
}

void generate_message(std::string &msg, message::request req){
    msg = message::serializeRequest(req);
    msg_id++;
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

    /* Starting epoll to watch input and recving*/
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

    /* Add epoll event input from user keyboard */
    ev.events = EPOLLIN;
    ev.data.fd = 0;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &ev) == -1){
        std::cerr << "[+] epoll_ctl waiting for user input failed" << "\n";
        close(epfd);
        exit(EXIT_FAILURE); 
    }
    
    /* Add epoll event receiving from server */
    ev.events = EPOLLIN;
    ev.data.fd = clientSock;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, clientSock, &ev) == -1){
        std::cerr << "[+] epoll_ctl recving from server failed" << "\n";
        close(epfd);
        exit(EXIT_FAILURE); 
    }

    /* Epoll operation */
    do{
        printf("\nAbout to epoll_wait(), type 'exit' to close:\n");
        if(!registered){
            printf("Type your name: \n");
        } else {
            printf("Type your message: \n");
        }
        event_count = epoll_wait(epfd, evlist, MAX_EVENTS, -1);
        if(event_count == -1){
            if(errno == EINTR)
                continue;
            else
                std::cerr << "epoll_wait!" << std::endl;
        }
        printf("[+] Event count: %d\n", event_count);
        for(int i=0; i<event_count; i++){
            printf("[+] Reading from file descriptor '%d' with \n", evlist[i].data.fd);
            if(evlist[i].data.fd == 0){
                bytes_read = read(evlist[i].data.fd, buffer, sizeof(buffer));
                buffer[bytes_read] = '\0';
                printf("[+] User > %s\n", buffer);
                /* Check exit */
                if(!strncmp(buffer, "exit\n", 5)){
                    running = 0;
                    close(clientSock);
                    break;
                } 
                
                /* Sending message */
                std::string msg;
                std::map<std::string, std::string> params;
                if(registered){
                    params.insert(std::pair<std::string, std::string>("message", std::string(buffer)));
                    generate_message(msg, create_request("server", "send", params));
                } else {
                    device_name = std::string(buffer);
                    generate_message(msg, create_request("server", "register", params));
                }
                /* Sending message */
                if(send(clientSock, msg.c_str(), msg.size(), 0) == -1){
                    printf("[+] Message sent failed!");
                } 
                printf("[+] Message sent: %s\n", msg.c_str());
            } else if(evlist[i].data.fd == clientSock){
                /* Reset buffer */
                bzero(buffer, sizeof(buffer));
                bytes_read = recv(clientSock, buffer, RECVBUFSIZE, 0);
                if(bytes_read == -1){
                    std::cout << "[+] There was an error getting a response from server!\n";
                } else {
                    printf("[+] Receive > %s\n", buffer);    
                }
                message::response resp = message::deserializeResponse(buffer);
                printf("[+] Message from server: %s\n", resp.message.c_str());
            }
        }
    } while(running);

    close(clientSock);
    exit(EXIT_SUCCESS);
}