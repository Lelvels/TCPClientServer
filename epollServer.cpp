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

#define RCVBUFSIZE 1024
#define MAX_EVENTS 10

static std::map<std::string, int> known_clients;
static int numFDs = 0;

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
handle_message(char* buffer, int fd){
    message::request req = message::deserializeRequest(buffer);
    message::response resp;
    printf("[+] Receive > %s\n", buffer);
    /* Registration */
    if(req.req_method.compare("register") == 0)
    {
        bool regis_failed = false;
        auto it = known_clients.find(req.device_name);
        if(it != known_clients.end()){
            resp.resp_code = RESP_REGISTRATION_FAILED;
            resp.message = "Registration failed due to duplicate device name!";
            regis_failed = true;
        } else {
            for(auto it=known_clients.begin(); it!=known_clients.end(); it++){
                if(it->second == fd){
                    resp.resp_code = RESP_REGISTRATION_FAILED;
                    resp.message = "Registration failed due to duplicate file descriptor!";
                    regis_failed = true; 
                }
            }
        }
        if(!regis_failed){
            known_clients.insert(std::pair<std::string, int>(req.device_name, fd));
            printf("[+] Client id: %s with fd %d registered!\n", req.device_name.c_str(), fd);
            resp.resp_code = RESP_REGISTRATION_OK;
            resp.message = "Registration succesfully!";
        }
    } 
    /* Send to other clients */
    else if(req.req_method.compare("send") == 0)
    {
        printf("[+] Reading send message\n");
        if(req.dest.compare("server") == 0){
            printf("[+] Message to server\n");
            for(auto it = req.params.begin(); it!=req.params.end(); it++){
                printf("\tKey: %s | Value: %s\n", it->first.c_str(), it->second.c_str());
            }
            resp.resp_code = RESP_OK;
            resp.message = "ACK";
        } else {
            printf("[+] Message to other client: %s", req.dest.c_str());
            //Check in the list client name !
            if(known_clients.count(req.dest) > 0){
                resp.resp_code = RESP_BAD_METHOD;
                resp.message = "Client id not exsists, please send to another client!";
            } else {
                //create noti for dest client
                message::notification noti;
                noti.from = req.device_name;
                noti.to = req.dest;
                noti.message = req.params;
                std::string noti_str = message::serializeNotification(noti);
                //Fire and forget :D
                write(known_clients.at(req.dest), noti_str.c_str(), noti_str.size());
                //Response back
                resp.message = "Message sent!";
                resp.resp_code = RESP_OK;
            }
        } 
    } 
    /* 404 Not Found */
    else {
        resp.resp_code = RESP_NOTFOUND;
        resp.message = "404 Method not found";
        printf("[+] Request method not found!\n");
    }
    
    /* Create Response and send back to client */
    resp.device_name = req.device_name;
    resp.id = req.id;
    std::string resp_str = message::serializeResponse(resp);
    write(fd, resp_str.c_str(), resp_str.size());
}

int main(int argc, char *argv[]){
    int serverSocket, clientSocket;
    struct sockaddr_in echoServAddr;
    struct sockaddr_in echoClntAddr;
    unsigned short echoServPort;
    unsigned int clntLen;

    int epfd, ready, buf_size;
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
        printf("\nAbout to epoll_wait()\n");
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
                printf("[+] Handling %d clients\n", numFDs);
            } else if(evlist[i].events & EPOLLIN){
                bzero(buf, sizeof(buf));
                buf_size = read(evlist[i].data.fd, buf, sizeof(buf));
                //if empty message => client disconnect!
                if(buf_size <= 0){
                    printf("[+] Disconnecting client with fd: %d!\n", evlist[i].data.fd);
                    for(auto it=known_clients.begin(); it!=known_clients.end(); it++){
                        if(it->second == evlist[i].data.fd){
                            known_clients.erase(it->first);
                        }
                    }
                    break;
                } else {
                    handle_message(buf, evlist[i].data.fd);
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
                printf("[+] Handling %d clients\n", numFDs);
				continue;
			}
        }
    } while(numFDs > 0);
    

    std::cout << "Server turn off!" << std::endl;
}