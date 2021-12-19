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
# define MAX_THREAD_CNT 3
# define MAX_QUEUE_CNT 10

struct Client {
    string address;
    string portNum;
    int sockfd;
    string acctName;
    int accountBalance;
    bool isOnline;

    void print();
};

// declare a global client list
vector<Client> CLIENT_LIST;

int CURRENT_CONNECTION = 0;

Client* findClient(string name);
string makeListInfo(Client* clientPtr);
int getOnlineNum();
void serving(void* clientPtr);
bool transaction(string senderName, string receiverName, int transAmount);

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
        
        if(CURRENT_CONNECTION < MAX_THREAD_CNT) {
            CURRENT_CONNECTION++;
            string successMsg = "Connection Succeeds\n";
            if (send(connfd, successMsg.c_str(), MAX_MSG_SIZE, 0) == -1) {
                cout << "send msg error: " << strerror(errno) 
                    << "(errno: " << errno << ")\n";
                return -1;
            }
            Client newClient;
            newClient.address = inet_ntoa(clientaddr.sin_addr);
            newClient.sockfd = connfd;
            while (threadpool_add(pool, &serving, (void*) &newClient, 0) != 0) {
                char msg[MAX_MSG_SIZE] = {"Exceeds Connection Limit\n"};
            }
        }
        else {
            string rejectMsg = "Exceeds Connection Limit\n";
            if (send(connfd, rejectMsg.c_str(), MAX_MSG_SIZE, 0) == -1) {
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
        bool transfer = false;
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
            for(int i = 0; i < CLIENT_LIST.size(); i++) {
                if(CLIENT_LIST[i].sockfd == client.sockfd) CLIENT_LIST[i].isOnline = false;
            }
            clientExit = true;
        }
        else if(message == "List") {
            // exception handling
            int onLine = false;
            for(int i = 0; i < CLIENT_LIST.size(); i++) {
                if(CLIENT_LIST[i].sockfd == client.sockfd) CLIENT_LIST[i].isOnline ? (onLine = true) : (onLine = false);
            }
            if(onLine) {
                Client* reqClientPtr;
                for(int i = 0; i < CLIENT_LIST.size(); i++) {
                    if(CLIENT_LIST[i].sockfd == client.sockfd) {
                        reqClientPtr = &CLIENT_LIST[i];
                        msgToSend = makeListInfo(reqClientPtr);
                        break;
                    }
                }
            }
            else {
                cout << "User is not logged in yet!\n";
                msgToSend = "Please login first";
            }
        }
        else if(count(message.begin(), message.end(), '#') == 2) {
            // transfer part
            transfer = true;
            string senderName = message.substr(0, message.find('#'));
            string temp = message.substr(message.find('#') + 1);
            int transAmount = stoi(temp.substr(0, temp.find('#')));
            string receiverName = temp.substr(temp.find('#') + 1);

            string transferMsg;
            if(transaction(senderName, receiverName, transAmount)) transferMsg = "Transfer OK!\n";
            else transferMsg = "Transfer Failed!\n";

            // sending message to sender
            Client* senderPtr = findClient(senderName);
            if(send(senderPtr->sockfd, transferMsg.c_str(), MAX_MSG_SIZE, 0) < 0) {
                cout << "send msg error: " << strerror(errno) 
                    << "(errno: " << errno << ")\n";
                __throw_bad_exception;
            }
            
        }
        else if(count(message.begin(), message.end(), '#') == 1){
            // login part
            string reqLoginName = message.substr(0, message.find("#"));
            Client* loginClientPtr = findClient(reqLoginName);
            if(loginClientPtr != nullptr) {
                loginClientPtr->address = client.address;
                loginClientPtr->portNum = message.substr(message.find("#") + 1);
                loginClientPtr->isOnline = true;
                loginClientPtr->sockfd = client.sockfd;

                client = *loginClientPtr;

                msgToSend = makeListInfo(loginClientPtr);
            }
            else msgToSend = "220 AUTH_FAIL\n";
        }
        // sending message to client
        if(!transfer) {
            if(send(client.sockfd, msgToSend.c_str(), MAX_MSG_SIZE, 0) < 0) {
                cout << "send msg error: " << strerror(errno) 
                    << "(errno: " << errno << ")\n";
                __throw_bad_exception;
            }
        }
    }
    close(client.sockfd);
    CURRENT_CONNECTION = CURRENT_CONNECTION - 1;
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

string makeListInfo(Client* clientPtr) {
    string listInfo;
    listInfo += to_string(clientPtr->accountBalance);
    listInfo += "\n";
    listInfo += "public key"; // server public key
    listInfo += "\n";
    listInfo += to_string(getOnlineNum());
    listInfo += "\n";

    for(int i = 0; i < CLIENT_LIST.size(); i++) {
        if(CLIENT_LIST[i].isOnline == true) {
            listInfo += CLIENT_LIST[i].acctName;
            listInfo += "#";
            listInfo += CLIENT_LIST[i].address;
            listInfo += "#";
            listInfo += CLIENT_LIST[i].portNum;
            listInfo += "\n";
        }
    }
    return listInfo;
}

int getOnlineNum() {
    int onlineCount = 0;
    for(int i = 0; i < CLIENT_LIST.size(); i++) {
        if(CLIENT_LIST[i].isOnline == true) onlineCount++;
    }
    return onlineCount;
}

void Client::print() {
    cout << this->acctName << '#'
         << this->address << '#'
         << this->portNum << '\n';
}

bool transaction(string senderName, string receiverName, int transAmount) {
    bool senderFind = false;
    bool receiverFind = false;
    for(int i = 0; i < CLIENT_LIST.size(); i++) {
        if(CLIENT_LIST[i].acctName.compare(senderName) == 0) {
            senderFind = true;
            CLIENT_LIST[i].accountBalance = CLIENT_LIST[i].accountBalance - transAmount;
        }
        else if(CLIENT_LIST[i].acctName.compare(receiverName) == 0) {
            receiverFind = true;
            CLIENT_LIST[i].accountBalance = CLIENT_LIST[i].accountBalance + transAmount;
        }
    }
    if(!senderFind || !receiverFind) return false;
    
    return true;
}