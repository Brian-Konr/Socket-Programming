#include <stdio.h>
#include <stdlib.h>
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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>

using namespace std;

# define MAX_MSG_SIZE 4096
# define MAX_THREAD_CNT 3
# define MAX_QUEUE_CNT 10

// RSA
char myKey[] = "./keys/server.key";
char myCert[] = "./keys/server.crt";
#define RSA_SERVER_KEY "keys/server.key"
#define RSA_SERVER_CERT "keys/server.crt"


// initialize and return SSL Content Tect (ctx)
SSL_CTX* InitServerCTX()
{
    SSL_CTX *ctx;
    /* SSL 庫初始化 */
    SSL_library_init();
    /* 載入所有 SSL 演算法 */
    OpenSSL_add_all_algorithms();
    /* 載入所有 SSL 錯誤訊息 */
    SSL_load_error_strings();
    /* 以 SSL V2 和 V3 標準相容方式產生一個 SSL_CTX ，即 SSL Content Text */
    ctx = SSL_CTX_new(SSLv23_server_method());
    /* 也可以用 SSLv2_server_method() 或 SSLv3_server_method() 單獨表示 V2 或 V3標準 */
    if (ctx == NULL)
    {
        ERR_print_errors_fp(stdout);
        abort();
    }

    string pid = to_string(getpid());
    cout<< pid;
    char cmd[1024];
    sprintf(cmd, "openssl req -x509 -sha256 -nodes -days 365 -newkey rsa:2048 -keyout ./keys/%s.key -out ./keys/%s.crt -subj \"/C=TW/ST=Taiwan/L=TPE/O=NTU/OU=IM/CN=Michael/emailAddress=\"", pid.c_str(), pid.c_str());
    system(cmd);
    cout<<"Certificate generated.";

    return ctx;
}

// pass initialized ctv, .key, and .crt into this function and it will load .key, .crt into ctx
void LoadCertificates(SSL_CTX* ctx)
{

    string pid = to_string(getpid());
    char key_path[1024];
    char cert_path[1024];
    sprintf(key_path, "./keys/%s.key", pid.c_str());
    sprintf(cert_path, "./keys/%s.crt", pid.c_str());

    /* 載入使用者的數字證書， 此證書用來發送給客戶端。 證書裡包含有公鑰 */
    if ( SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* 載入使用者私鑰 */
    if ( SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* 檢查使用者私鑰是否正確 */
    if ( !SSL_CTX_check_private_key(ctx) )
    {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }
}

// print certificates of others
void ShowCerts(SSL *ssl)
{
    X509 *cert;
    char *line;

    cert = SSL_get_peer_certificate(ssl);
    if (cert != NULL)
    {
        printf("Digital certificate information:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Certificate: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);
        X509_free(cert);
    }
    else
        printf("No certificate information!\n");
}


// client structure
struct Client {
    string address;
    string portNum;
    int sockfd;

    // SSL replaces the sockfd
    SSL* ssl;

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

    SSL_CTX* ctx = InitServerCTX();
    LoadCertificates(ctx);

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

        /* 將連線使用者的 socket (connfd) 加入到 SSL */
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, connfd);

        /* 建立 SSL 連線 */
        if(SSL_accept(ssl) == -1) {
            cout << "error in SSL accept" << endl;
            return -1;
        }

        

        // TODO: 要改成 SSL_Read, SSL_Write 
        
        if(CURRENT_CONNECTION < MAX_THREAD_CNT) {
            CURRENT_CONNECTION++;
            string successMsg = "Connection Succeeds\n";
            SSL_write(ssl, successMsg.c_str(), MAX_MSG_SIZE);
            // if (send(connfd, successMsg.c_str(), MAX_MSG_SIZE, 0) == -1) {
            //     cout << "send msg error: " << strerror(errno) 
            //         << "(errno: " << errno << ")\n";
            //     return -1;
            // }
            Client newClient;
            newClient.address = inet_ntoa(clientaddr.sin_addr);
            newClient.sockfd = connfd;

            // 將 accept 完 的 ssl 加到 client 的 ssl
            newClient.ssl = ssl;
            
            while (threadpool_add(pool, &serving, (void*) &newClient, 0) != 0) {
                char msg[MAX_MSG_SIZE] = {"Exceeds Connection Limit\n"};
            }
        }
        else {
            string rejectMsg = "Exceeds Connection Limit\n";
            SSL_write(ssl, rejectMsg.c_str(), MAX_MSG_SIZE);
        }
    }
}

void serving(void* clientPtr) {
    Client client = *(Client*) clientPtr;
    cout << "new server thread creation, and its connection fd is " << &client.ssl << endl;
    bool clientExit = false;

    while(!clientExit) {
        bool transfer = false;
        // receive messages from client
        char rcvMsg[MAX_MSG_SIZE] = {0};
        SSL_read(client.ssl, rcvMsg, MAX_MSG_SIZE);
        cout << "receive message from client: " << rcvMsg << endl;

        string msgToSend;
        string message = rcvMsg;

        if(message.substr(0, 8) == "REGISTER") {
            cout << "register" << endl;
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
                        cout << "msgToSend: " << endl << msgToSend;
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
            SSL_write(senderPtr->ssl, transferMsg.c_str(), MAX_MSG_SIZE);
            
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
                loginClientPtr->ssl = client.ssl;

                client = *loginClientPtr;

                msgToSend = makeListInfo(loginClientPtr);
            }
            else msgToSend = "220 AUTH_FAIL\n";
        }
        // sending message to client
        if(!transfer) {
            cout << "ready to send message: " << msgToSend;
            SSL_write(client.ssl, msgToSend.c_str(), MAX_MSG_SIZE);
        }
    }
    SSL_shutdown(client.ssl);
    SSL_free(client.ssl);
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