#include<iostream>
#include<sys/types.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<string.h>
#include<string>
#include<pthread.h>

#define MAX_PENDING 5
#define RCVBUFSIZE 32 

struct ThreadArgs {
    int clntSock;
};

void DieWithError(char* errorMsg){
    std::cerr << errorMsg << "\n";
    return;
}

void HandleTCPClient(int clntSocket){
    char echoBuffer[RCVBUFSIZE];
    int recvMsgSize;

    if((recvMsgSize = recv(clntSocket, echoBuffer, RCVBUFSIZE, 0)) < 0){
        char err_msg[] = "Recv() failed!"; 
        DieWithError(err_msg);
    } else {
        std::cout << "Receiving: " << echoBuffer << "\n";
    }

    while (recvMsgSize > 0)
    {
        std::string msg = "200";
        if(send(clntSocket, msg.c_str(), sizeof(msg), 0) != recvMsgSize){
            char err_msg[] = "send() failed!"; 
            DieWithError(err_msg);
        } else {
            std::cout << "Sending response" << "\n";
        }
        
        if((recvMsgSize = recv(clntSocket, echoBuffer, RCVBUFSIZE, 0)) < 0){
            char err_msg[] = "recv() failed!"; 
            DieWithError(err_msg);
        } else {
            std::cout << "Receiving: " << echoBuffer << "\n";
        }
    }

    close(clntSocket);
}

void *ThreadMain(void *threadArgs){
    int clntSock; /* Socket description for client */
    pthread_detach(pthread_self());  /* Guarantees that thread resources are deallocated upon return */
    clntSock = ((struct ThreadArgs *)threadArgs)->clntSock;
    free(threadArgs);
    HandleTCPClient(clntSock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){
    int servSock;
    int clntSock;
    struct sockaddr_in echoServAddr;
    struct sockaddr_in echoClntAddr;
    unsigned short echoServPort;
    unsigned int clntLen;
    pthread_t threadID;
    struct ThreadArgs *threadArgs;

    // Check if the number of args is enough
    if(argc != 2){
        std::cerr << "Usage server port: " << argv[0] << "\n";
        exit(1);
    }
    
    echoServPort = atoi(argv[1]); /* Taking port number from args */

    if((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){ /* Creating server socket */
        char err_msg[] = "Create socket() failed!\n"; 
        DieWithError(err_msg);
    }
    const int enable = 1;
    if (setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        char err_msg[] = "setsockopt(SO_REUSEADDR) failed";
        DieWithError(err_msg);
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(echoServPort);
    
    if(bind(servSock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0){
        char err_msg[] = "bind() serv address failed !";
        DieWithError(err_msg);
    }

    if(listen(servSock, MAX_PENDING) < 0){
        char err_msg[] = "listen() serv address failed !";
        DieWithError(err_msg);
    }

    for(;;){
        clntLen = sizeof(echoClntAddr);
        if((clntSock = accept(servSock, (struct sockaddr *) &echoClntAddr, &clntLen)) < 0){
            char err_msg[] = "bind() serv address failed !";
            DieWithError(err_msg);
        }

        if((threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL){
            char err_msg[] = "Cannot allocate memory for thread args";
            DieWithError(err_msg);
        }
        threadArgs -> clntSock = clntSock;

        /* Create client thread */
        if(pthread_create(&threadID, NULL, ThreadMain, (void *) threadArgs) != 0){
            char err_msg[] = "Cannot create new thread";
            DieWithError(err_msg);
        } else {
            std::cout << "Create new thread with " << threadID << " and client socket: " << clntSock << std::endl;
            printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));
        }
    }

    std::cout << "Server turn off!" << std::endl;
}