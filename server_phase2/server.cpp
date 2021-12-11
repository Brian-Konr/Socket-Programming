# include <iostream>
# include <string.h>
# include <pthread.h>
# include <errno.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <algorithm>
# include <queue>
# include <vector>
# include <assert.h>
# include "threadpool.c"
# include "threadpool.h"

using namespace std;

# define MAX_MSG_SIZE 4096
# define MAX_THREAD_CNT 2
# define MAX_QUEUE_CNT 10

struct Client {
    string address;
    int portnum;
    int sockfd;
    string acctName;
    int accountBalance;
    bool isOnline;
};

struct ClientList {
    int onlineNum;
    vector<Client> list;
    Client* find(string);
    // string onlineliststr(Client*);
};

// ClientList clientList;
vector<Client> CLIENT_LIST;
Client* findClient(string name);
void serving(void* clientPtr);

int main() {
    // serverPortNum typing
    int serverPortNum = 8888;
    cout << "Please enter the server port: ";
    cin >> serverPortNum;

    // build a listen socket
    int listenfd, connfd;
    struct sockaddr_in servaddr, clientaddr;

    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        cout << "socket creation failed" << endl;
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(serverPortNum);

    // threadpool
    threadpool_t* pool;
    assert((pool = threadpool_create(MAX_THREAD_CNT, MAX_QUEUE_CNT, 0)) != NULL);

    // bind
    if(bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
        cout << "error in socket binding" << endl;
        return -1;
    }

    // listen
    if (listen(listenfd, MAX_QUEUE) == -1) {
        cout << "listen socket error" << endl;
        return -1;
    }

    cout << "The server is ready for incoming connection!" << endl;

    while(1) {
        // accept
        int clientaddrLen = sizeof(clientaddr);
        if((connfd = accept(listenfd, (struct sockaddr*) &clientaddr, (socklen_t*) &clientaddrLen)) < 0) {
            cout << "error in accepting client" << endl;
            return -1;
        }

        // handle new client
        Client newClient;
        newClient.address = inet_ntoa(clientaddr.sin_addr);
        newClient.sockfd = connfd;
        while (threadpool_add(pool, &serving, (void*) &newClient, 0) != 0) {
            char msg[MAX_MSG_SIZE] = {"buffering\n"};
            if (send(connfd, msg, sizeof(msg), 0) == -1) {
                cout << "send msg error: " << strerror(errno) 
                     << "(errno: " << errno << ")\n";
                return -1;
            }
        }
    }
}

void serving(void* clientPtr) {
    Client client = *(Client*) clientPtr;
    cout << "new server thread creation, and its connection fd is " << client.sockfd << endl;
    bool clientExit = false;

    while(!clientExit) {
        // receive messages from client
        char rcvMsg[MAX_MSG_SIZE] = {0};
        recv(client.sockfd, rcvMsg, MAX_MSG_SIZE, 0);
        cout << "receive message from client: " << rcvMsg << endl;

        string msgToSend;
        string message = rcvMsg;
        if(message.substr(0, 8) == "REGISTER") {
            string reqRegisterName = message.substr(message.find("#") + 1);
            if(findClient(reqRegisterName) == nullptr) {
                Client registerClient;
                registerClient.acctName = message.substr(message.find("#") + 1);
                registerClient.accountBalance = 10000; // initialization
                registerClient.isOnline = false; // need to login to become true
                CLIENT_LIST.push_back(registerClient);

                msgToSend = "100 OK\n";
            }
            else {
                // cout << "username already existed!" << endl;
                msgToSend = "210 FAIL\n";
            }
        }
        else if(message == "Exit") {
            msgToSend = "Bye\n";
        }
        // sending message to client
        if(send(client.sockfd, msgToSend.c_str(), sizeof(msgToSend), 0) < 0) {
            cout << "send msg error: " << strerror(errno) 
                << "(errno: " << errno << ")\n";
            __throw_bad_exception;
        }
    }
    close(client.sockfd);
    cout << "Client has disconnected" << endl;
}

Client* findClient(string name) {
    if (CLIENT_LIST.empty())
        return nullptr;
    for (int i = 0; i < CLIENT_LIST.size(); i++) {
        if (CLIENT_LIST[i].acctName.compare(name) == 0) {
            return &(CLIENT_LIST[i]);
        }
    }
    return nullptr;
}