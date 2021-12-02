# include <iostream>
# include <string.h>
# include <string.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <typeinfo>
using namespace std;

int PORT;
int SERVER_CLIENT_SOCKETFD;
string USER_NAME;
# define MAX_MSG_SIZE 4096
# define MAX_CLIENT 10

void* receive_thread(void* server_fd);
void receiving(int server_fd);
void transfering(char* toSendAddress, int payee_port, string msg);
int receiveList(char* buffer); /*bool search = false, string targetName = ""*/
int main() {

    /*First, build a socket to listen incoming messages from other clients*/
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int k = 0;

    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    cout << "Enter your port number: ";
    cin >> PORT;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind
    if(bind(server_fd, (struct sockaddr*) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // listen
    if(listen(server_fd, MAX_CLIENT) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    pthread_t tid;
    pthread_create(&tid, NULL, &receive_thread, &server_fd);
    
    /*Next, we have to build the connection between client and server */
    int socketfd;
    struct sockaddr_in servaddr;

    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0) {
        cout << "error in creating socket";
        return -1;
    }
    char server_address[100];
    int server_port_num;
    cout << "Please enter the server address: ";
    cin >> server_address;
    cout << "Please enter the server port: ";
    cin >> server_port_num;

    servaddr.sin_addr.s_addr = inet_addr(server_address);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(server_port_num);

    if(connect(socketfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }

    SERVER_CLIENT_SOCKETFD = socketfd; // global variable for listen thread to use

    cout << "connection succeeds" << endl;
    bool exit = false, isLogin = false;
    while(!exit) {
        string request, sendMessage;
        char buffer[MAX_MSG_SIZE] = {0}; //used to receive from server
        string welcomeMsg;
        welcomeMsg = isLogin ? USER_NAME + ", what service do you want today? " : "Hello, what service do you want today? ";
        cout << welcomeMsg;
        cin >> request;
        if(request == "register") {
            string userAccountName;
            cout << "Please type the username you want: ";
            cin >> userAccountName;
            sendMessage = "REGISTER#" + userAccountName;
        }
        else if(request == "login") {
            if(!isLogin) {
                string userAccountName;
                cout << "Please enter your user account name: ";
                cin >> userAccountName;
                USER_NAME = userAccountName;
                sendMessage = userAccountName + "#" + to_string(PORT);
            }
            else {
                cout << "You have logged in!" << endl;
            }
        }
        else if(request == "list") {
            if(isLogin) sendMessage = "List";
            else cout << "Please login first!" << endl;
        }
        else if(request == "exit") {
            char exitBuffer[MAX_MSG_SIZE] = {0};
            strcpy(exitBuffer, "Exit");
            write(socketfd, exitBuffer, sizeof(exitBuffer));
            read(socketfd, exitBuffer, sizeof(exitBuffer));
            if(strcmp(exitBuffer, "Bye\n") == 0 || true) {
                cout << "successfully terminate!" << endl;
                if(isLogin) cout << USER_NAME << ", See you next time!";
                else cout << "See you next time!";
                cout << endl;
                return 0;
            }
        }
        else if(request == "transfer") {
            if(isLogin) {
                string msg = "List";
                char tempBuffer[MAX_MSG_SIZE] = {0};
                char contBuffer[MAX_MSG_SIZE] = {0};
                send(socketfd, msg.c_str(), sizeof(msg), 0);
                recv(socketfd, tempBuffer, MAX_MSG_SIZE, 0);
                strcpy(contBuffer, tempBuffer);
                if(receiveList(tempBuffer) == -1) cout << "information from server has packet loss, please type transfer to send and receive info again" << endl;
                else {
                    int continue_ = 2; // default not continue
                    cout << "Do you want to continue? 1: yes, 2: no ";
                    cin >> continue_;
                    if(continue_ == 1) {
                        string payeeName;
                        int payeePort;
                        string userName;
                        string payment;
                        cout << USER_NAME << ", Please enter your name again to confirm this transaction: ";
                        cin >> userName;
                        cout << "Please enter the username you want to transfer to: ";
                        cin >> payeeName;
                        cout << userName << ", Please enter the amount of money you want to transfer to " << payeeName << ": ";
                        cin >> payment;
                        bool find = false;

                        // find the payee;
                        int count = 0;
                        char* p;
                        const char* delim = "\n";
                        p = strtok(contBuffer, delim);
                        string receiveArr[MAX_CLIENT + 3];
                        while(p != NULL) {
                            receiveArr[count] = p;
                            count++;
                            p = strtok(NULL, delim);
                        }
                        for(int i = 3; i < count; i++) {
                            const char* newDelim = "#";
                            char* line = (char*)receiveArr[i].c_str();
                            char* p = strtok(line, "#");
                            if(strcmp(p, payeeName.c_str()) == 0) {
                                find = true;
                                p = strtok(NULL, "#");
                                char payeeAddress[100] = {0};
                                strcpy(payeeAddress, p);
                                p = strtok(NULL, "#");
                                char portArr[100] = {0};
                                strcpy(portArr, p);
                                string msg = userName + "#" + payment + "#" + payeeName;
                                transfering(payeeAddress, atoi(portArr), msg);
                            }
                        }
                        if(!find) cout << payeeName << " NOT FOUND!" << endl;
                    }
                }
                sendMessage = "";

                // connect the payee
                // transfering();
            }
            else cout << "Please login first!" << endl;
        }
        // send messages
        if(!sendMessage.empty() && send(socketfd, sendMessage.c_str(), sizeof(sendMessage), 0) < 0) { // c_str trans string to char array, because the params needs this type
            cout << "Error in sending messages";
            return -1;
        }
        if(!sendMessage.empty()) {
            recv(socketfd, buffer, MAX_MSG_SIZE, 0);
            if(request == "list") {
                if(buffer == "Please login first\n") cout << buffer;
                else if(receiveList(buffer) == -1) cout << "info from server is not complete" << endl;
            }
            else if(request == "login") {
                char temp[13];
                strncpy(temp, buffer, 13);
                temp[13] = '\0';
                if(strcmp(temp, "220 AUTH_FAIL") == 0) cout << buffer << "invalid login request!" << endl;
                else {
                    isLogin = true;
                    cout << "successfully logged in" << endl << endl;
                }
                if(receiveList(buffer) == -1) cout << "info from server is not complete" << endl;
            }
            else cout << buffer;
        }
    }
    
    close(socketfd);
    return 0;
}
void transfering(char* toSendAddress, int payee_port, string msg) {
    int payee_socketfd;
    struct sockaddr_in payeeaddr;

    if((payee_socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "socket creation error";
        return;
    }

    payeeaddr.sin_addr.s_addr = inet_addr(toSendAddress);
    payeeaddr.sin_family = AF_INET;
    payeeaddr.sin_port = htons(payee_port);

    // connect
    if(connect(payee_socketfd, (struct sockaddr*) &payeeaddr, sizeof(payeeaddr)) < 0) {
        cout << "error in connection to payee" << endl;
        return;
    }
    // send message to payee
    // string msg;
    // cout << "Please enter the message to be sent: ";
    // cin >> msg;
    if(send(payee_socketfd, msg.c_str(), sizeof(msg), 0) < 0) {
        cout << "error in sending message to payee" << endl;
        return;
    }
    else cout << "message successfuly sent!" << endl;
    close(payee_socketfd);
}
// calling receiving every 2 seconds
void* receive_thread(void* server_fd) {
    int s_fd = *((int*) server_fd);
    while(1) {
        sleep(2);
        receiving(s_fd);
    }
}

// receive messages on our port
void receiving(int server_fd) {
    struct sockaddr_in address;
    int valread;
    char buffer[2000] = {0};
    int addrlen = sizeof(address);
    fd_set set;

    //Initialize my current set
    FD_ZERO(&set);
    int k = 0;
    FD_SET(server_fd, &set);
    if (select(FD_SETSIZE, &set, NULL, NULL, NULL) < 0)
    {
        perror("Error");
        exit(EXIT_FAILURE);
    }
    if (FD_ISSET(server_fd, &set)) {
        int client_socket;
        if ((client_socket = accept(server_fd, (struct sockaddr *) &address,
                                    (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        bzero(buffer, sizeof(buffer));
        char duplicate[MAX_MSG_SIZE] = {0};
        char receiveBuff[MAX_MSG_SIZE] = {0};
        recv(client_socket, buffer, sizeof(buffer), 0);
        strcpy(duplicate, buffer);
        char* p;
        string from_name, amount;
        p = strtok(duplicate, "#");
        from_name = p;
        p = strtok(NULL, "#");
        amount = p;
        if(send(SERVER_CLIENT_SOCKETFD, buffer, sizeof(buffer), 0) < 0) {
            cout << from_name << " wants to transfer " << amount << " to you, but something went wrong..." << endl;
        }
        else {
            recv(SERVER_CLIENT_SOCKETFD, receiveBuff, MAX_MSG_SIZE, 0);
            cout << receiveBuff;
            cout << "------There is a new transaction coming!------" << endl;
            cout << from_name << " had just transfered " << amount << " to you! Go check your new account balance!" << endl; 
        }
        return;
    }
}
int receiveList(char* buffer) { /*bool search = false, string targetName = ""*/
    int lineCount = 0; // record the lines received from server
    const char* delim = "\n";
    char* p;
    p = strtok(buffer, delim);
    string receiveArr[MAX_CLIENT + 3];
    string wrongMsg = "info from server is not complete";
    while(p != NULL) {
        receiveArr[lineCount] = p;
        lineCount++;
        // cout << "line " << lineCount << ": " << p << endl;
        p = strtok(NULL, delim);
    }
    if(lineCount < 3) return -1;
    if(receiveArr[2] == "" || receiveArr[2].length() > 1) return -1;
    if(stoi(receiveArr[2]) != lineCount - 3) return -1;

    cout << "------list information------" << endl;
    cout << "Account Balance: " << receiveArr[0] << endl;
    cout << "Server's public key: " << receiveArr[1] << endl;
    cout << "Number of accounts online: " << receiveArr[2] << endl;
    for(int i = 3; i < lineCount; i++) {
        cout << "Account " << i-2 << "'s info: " << receiveArr[i] << endl;
    }
    cout << "----------------------------" << endl << endl;
    return 0;
}