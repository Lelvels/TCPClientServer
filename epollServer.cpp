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

#include"common.h"

#define RCVBUFSIZE 512
#define MAX_EVENTS 10

void errExit(std::string errorMsg){
    std::cerr << errorMsg << "\n";
}

static int 
setnonblocking(int sockfd)
{
    if(fcntl(sockfd, F_SETFD, fcntl(sockfd, F_GETFD, 0), 0) | O_NONBLOCK == -1){
        return -1;
    }
    return 0;
}

static void 
epoll_ctl_add(int epfd, int fd, uint32_t events){
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1){
        perror("epoll_ctl() err\n");
        exit(1);
    }
}

static void 
handle_message(char* buffer){
    printf("[+] Receiving message: %s", buffer);
}

int main(int argc, char *argv[]){
    int serverSocket, clientSocket;
    struct sockaddr_in echoServAddr;
    struct sockaddr_in echoClntAddr;
    unsigned short echoServPort;
    unsigned int clntLen;

    int epfd, ready, buf_size, numFDs = 0;
    struct epoll_event ev;
    struct epoll_event evlist[MAX_EVENTS];
    char buf[RCVBUFSIZE];

    // Check if the number of args is enough
    if(argc != 2){
        std::cerr << "Usage server port: " << argv[0] << "\n";
        exit(1);
    }
    
    echoServPort = atoi(argv[1]); /* Taking port number from args */

    if((serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){ /* Creating server socket */ 
        errExit("Create socket() failed!\n");
    }
    const int enable = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        errExit("setsockopt(SO_REUSEADDR) failed");
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(echoServPort);
    
    if(bind(serverSocket, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0){
        errExit("bind() serv address failed !");
    }

    setnonblocking(serverSocket);
    if(listen(serverSocket, MAX_EVENTS) < 0){
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

    /* 1. Add server socket to interesting list */
    epoll_ctl_add(epfd, serverSocket, EPOLLIN | EPOLLOUT | EPOLLET);
    
    do {
        /* Fetch up to MAX_EVENT items from the ready list */
        printf("About to epoll_wait()\n");
        ready = epoll_wait(epfd, evlist, MAX_EVENTS, -1);
        if(ready == -1){ 
            errExit("epoll_wait() error");
        }

        printf("[+] Ready events: %d\n", ready);

        for(int i = 0; i< ready; i++){
            printf("[+] Getting event fd=%d; events: %s%s%s\n", evlist[i].data.fd,
                (evlist[i].events & EPOLLIN) ? "EPOLLIN " : "",
                (evlist[i].events & EPOLLHUP) ? "EPOLLHUP " : "",
                (evlist[i].events & EPOLLERR) ? "EPOLLERR " : "");
            if(evlist[i].data.fd == serverSocket){
                unsigned int clientLen = sizeof(echoClntAddr);
                if((clientSocket = accept(serverSocket, (struct sockaddr *) &echoClntAddr, &clientLen)) < 0){
                    std::cerr << "Cannot accept client!" << errno << std::endl;
                } else {
                    printf("[+] Accept client on fd: %d", clientSocket);
                }
                inet_ntop(AF_INET, (char *)&(echoClntAddr.sin_addr),
					buf, sizeof(echoClntAddr));
				printf(", connected with %s:%d\n", buf,
				       ntohs(echoClntAddr.sin_port));
                setnonblocking(clientSocket);
                epoll_ctl_add(epfd, clientSocket, EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
                numFDs++;
            } else if(evlist[i].events & EPOLLIN){
                bzero(buf, sizeof(buf));
                buf_size = read(evlist[i].data.fd, buf, sizeof(buf));
                if(buf_size <= 0){
                    break;
                } else {
                    handle_message(buf);
                    write(evlist[i].data.fd, "200\0", 4);
                }
            } else {
                printf("[+] Unexpected event\n");
            }
            /* check if the connection is closing */
			if (evlist[i].events & (EPOLLRDHUP | EPOLLHUP)) {
				printf("[+] Connection closed with fd: %d\n", evlist[i].data.fd);
				epoll_ctl(epfd, EPOLL_CTL_DEL,
					  evlist[i].data.fd, NULL);
				close(evlist[i].data.fd);
                numFDs--;
				continue;
			}
        }
    } while(numFDs > 0);
    

    std::cout << "Server turn off!" << std::endl;
}