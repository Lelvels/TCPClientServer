#include<iostream>
#include<sys/types.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<string.h>
#include<string>

#define RCVBUFSIZE 32 

int creatSocket(void){
    std::cout << "Creating socket" << std::endl; 
    int clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    return clientSock;
}

int connectToServer(int clientSock){
    int echoServPort = 54000;
    std::string ipAddress = "127.0.0.1";
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(echoServPort);
    inet_pton(AF_INET, ipAddress.c_str(), &hint.sin_addr);

    int connectResult = connect(clientSock, (sockaddr *) &hint, sizeof(hint));
    return connectResult;
}

int sendNameToServer(int clientSock, std::string userName){
    int result = 0;
    int sendResult = send(clientSock, userName.c_str(), sizeof(userName), 0);
    char buffer[RCVBUFSIZE];
    memset(buffer, 0, sizeof(buffer));
    int bytesReceive = recv(clientSock, buffer, RCVBUFSIZE, 0);
    result = sendResult && bytesReceive;
    return result;
}

int main(){
    using namespace std;

    int clientSock = creatSocket();
    if(clientSock == -1)
    {
        cerr << "Can't create a socket()";
        return 1;
    }
 
    int connectResult = connectToServer(clientSock);
    if(connectResult < 0)
    {
        std::cout << "Connect to server fail!\n";
        return 1;
    }

    //4.
    char buffer[RCVBUFSIZE];
    string userInput;
    string userName;
    cout << "Enter your name >";
    getline(cin, userName);
    int identifyResult = sendNameToServer(clientSock, userName);
    if(identifyResult == -1){
        cerr << "Cannot identify from server, connection failed!";
    } else {
        cout << "Indentify successfully! Start messaging" << endl;
        do 
        {
            cout << "Enter a line >";
            getline(cin, userInput);

            int sendResult = send(clientSock, userInput.c_str(), sizeof(userInput), 0);
            if(sendResult == -1){
                cerr << "Can't send to server!\r\n";
                continue;
            }

            memset(buffer, 0, RCVBUFSIZE);
            int bytesReceive = recv(clientSock, buffer, RCVBUFSIZE, 0);
            if(bytesReceive == -1){
                cout << "There was a error getting a response from server!\n";
            } else {
                cout << "SERVER > " << string(buffer, bytesReceive) << "\r\n";
            }
        } 
        while (true);
    }
    
    
    //5.
    close(clientSock);
    return 0;
}