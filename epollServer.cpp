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
#include<errno.h>

#define RCVBUFSIZE 512
#define MAX_EVENTS 10

void errExit(std::string errorMsg){
    std::cerr << errorMsg << "\n";
}

int main(int argc, char *argv[]){
    int servSock, clntSock;
    struct sockaddr_in echoServAddr;
    struct sockaddr_in echoClntAddr;
    unsigned short echoServPort;
    unsigned int clntLen;

    int epfd, ready, fd, s, j, numOpenFds;
    struct epoll_event ev;
    struct epoll_event evlist[MAX_EVENTS];

    // Check if the number of args is enough
    if(argc != 2){
        std::cerr << "Usage server port: " << argv[0] << "\n";
        exit(1);
    }
    
    echoServPort = atoi(argv[1]); /* Taking port number from args */

    if((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){ /* Creating server socket */ 
        errExit("Create socket() failed!\n");
    }
    const int enable = 1;
    if (setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        errExit("setsockopt(SO_REUSEADDR) failed");
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(echoServPort);
    
    if(bind(servSock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0){
        errExit("bind() serv address failed !");
    }

    if(listen(servSock, MAX_EVENTS) < 0){
        errExit("listen() serv address failed !");
    }

    //Creating epoll
    epfd = epoll_create(MAX_EVENTS);
    if(epfd == -1){
        errExit("epoll create failed");
    }

    /* Procedure:
    1. Put server socket to interesting evlist
    2. Create a loop that do this that make epoll ready to listen to the evlist. 
    3. When it ready, check what is the event ?
    4. Check the event */

    /* 1. Add server to interesting list */
    ev.events = EPOLLIN;
    ev.data.fd = servSock;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, servSock, &ev) == -1){
        errExit("epoll_ctl");
    }

    do {
        /* Fetch up to MAX_EVENT items from the ready list */
        printf("About to epoll_wait()\n");
        ready = epoll_wait(epfd, evlist, MAX_EVENTS, -1);
        if(ready == -1){
            if(errno == EINTR)
                continue;
            else 
                errExit("epoll_wait");
        }

        printf("Ready: %d\n", ready);

        /* Deal with returned list of events */
        for(j = 0; j < ready; j++){
            printf("fd=%d; events: %s%s%s\n", evlist[j].data.fd,
                (evlist[j].events & EPOLLIN) ? "EPOLLIN " : "",
                (evlist[j].events & EPOLLHUP) ? "EPOLLHUP " : "",
                (evlist[j].events & EPOLLERR) ? "EPOLLERR " : "");
            /*  After waiting, epoll create an event list that contain all the event that it listen on the interesting list
                
                If there is no connection to the server, then it will only listen to the server socket, which only accept!

                When there is an connection comes in, the server will accepts this then return a file descriptor of the client.
                We will add this to the monitor, to manage this client socket.
                    EPOLLIN: The associated file is available for read(2) operations.
                    EPOLLHUP | EPOLLERR: If EPOLLIN and EPOLLHUP were both set, then there might be more than MAX_BUF bytes to read. Therefore, we close the file descriptor only if EPOLLIN was not set. We'll read further bytes after the next epoll_wait().
            */
            if(evlist[j].events & EPOLLIN){
                if(evlist[j].data.fd == servSock){
                    unsigned int clientLen = sizeof(echoClntAddr);
                    if((clntSock = accept(servSock, (struct sockaddr *) &echoClntAddr, &clientLen)) < 0){
                        std::cerr << "Cannot accept client!" << errno << std::endl;
                    }
                    ev.events = EPOLLIN;
                    ev.data.fd = clntSock;
                    if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1){
                        errExit("Cannot add client to interesting list");
                        //TODO: Add an error reader here!
                    } else {
                        std::cout << "Client Sock added: " << clntSock << std::endl;
                        // Sending message to client after connect
                        char msg[] = "Hello from server!\0";
                        if(send(clntSock, msg, sizeof(msg), 0) == -1){
                            std::cerr << "Send greeting failed!" << "\n";
                        } else {
                            std::cout << "Send greeting response!" << "\n";
                        }
                    }    
                } else {
                    char echoBuffer[RCVBUFSIZE];
                    int clientSocket = evlist[j].data.fd;
                    int recvMsgSize = 0;
                    if((recvMsgSize = recv(clientSocket, echoBuffer, RCVBUFSIZE, 0)) < 0){
                        char err_msg[] = "recv() failed!"; 
                        std::cerr << err_msg << std::endl;
                    } else {
                        std::cout << "Receiving: " << echoBuffer << "\n";
                    }
                }
            } else if(evlist[j].events & (EPOLLHUP || EPOLLERR)){
                printf(" closing fd %d\n", evlist[j].data.fd);
                if(close(evlist[j].data.fd) == -1)
                    errExit("close");
            }
        }
    } while(true);
    

    std::cout << "Server turn off!" << std::endl;
}