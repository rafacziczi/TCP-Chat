#include "header.h"

struct User {
    SOCKET userSocket;
    std::string name;
    unsigned char pub_key[crypto_box_PUBLICKEYBYTES];
    
    User(SOCKET userSocket, std::string name, const unsigned char* key)
        : userSocket(userSocket), name(name) {
            memcpy(pub_key, key, crypto_box_PUBLICKEYBYTES);
        }
};


class Server {
private:
    SOCKET serverSocket;
    sockaddr_in serverAddress;
    std::vector<User> users;
    std::mutex usersMutex;
public:
    Server(int port);
    ~Server();
    void start();
    void send_msg(SOCKET, const std::string&, Message);
    int receiver_in(const std::string&);
    void handle_conn(SOCKET);
    Message receive(SOCKET);
    SOCKET find_socket(const std::string&);
    std::string find_name(const SOCKET);
    void find_key(unsigned char*, const std::string&);
};

Server::Server(int port) {
    WSADATA wsaData;


    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
}

Server::~Server() {
    if(serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
    }
    WSACleanup();
}


void Server::start() {
    if(bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)
        ) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        return;
    }


    sockaddr_in localAddr;
    int len = sizeof(localAddr);

    getsockname(serverSocket, (sockaddr*)&localAddr, &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &localAddr.sin_addr, ip, sizeof(ip));

    std::cout << "Server listening on " << ip << ":" 
    << ntohs(localAddr.sin_port) << std::endl;

    
    
    if(listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        return;
    }

    while(true) {
        sockaddr addr;
        int addrlen = sizeof(addr);
        SOCKET clientSocket = accept(serverSocket, &addr, &addrlen);
        if(clientSocket != INVALID_SOCKET) {
            std::thread clientThread(&Server::handle_conn, this, clientSocket);
            clientThread.detach();
        }
        else {
            std::cerr << "Accept failed" << std::endl;
        }
    }
}

SOCKET Server::find_socket(const std::string& name) {
    for(const auto& user : users) {
        if(user.name == name) {
            return user.userSocket;
        }
    }

    return INVALID_SOCKET;
}

std::string Server::find_name(const SOCKET clientSocket) {
    for(const auto& user: users) {
        if(user.userSocket == clientSocket) {
            return user.name;
        }
    }

    return "";
}


void Server::find_key(unsigned char* output, const std::string& name) {
    for(const auto& user: users) {
        if(user.name == name) {
            memcpy(output, user.pub_key, crypto_box_PUBLICKEYBYTES);
        }
    }
}



int Server::receiver_in(const std::string& receiver) {
    for (int i = 0; i < users.size(); ++i) {
        if (users[i].name == receiver) {
            return i;
        }
    }
    return -1;
}

void Server::handle_conn(SOCKET clientSocket) {
    bool logged = false;
    Message firstMsg = receive(clientSocket);
    std::string clientName;

    if(firstMsg == Message::INVALID) {
        std::cerr << "Error receiving message" << std::endl;

        closesocket(clientSocket);
        return;
    }

    if(firstMsg.type == "login") {
        {
        std::lock_guard<std::mutex> lock(usersMutex); //"login [nick] [ignore]"
        //Kazda kolejna wiadomosc zaczyna sie automatycznie od nicku nadawcy
        //Tylko w pierwszej wiadomosci client.cpp wysyla swoj nick, dalej przypisuje go server.cpp
        if(receiver_in(firstMsg.sender) != -1) {
            std::cerr << "User " << firstMsg.sender << " already logged in." << std::endl;
            closesocket(clientSocket);
            return;
        }

        std::string pubkey_hex = firstMsg.data;
        unsigned char pubkey[crypto_box_PUBLICKEYBYTES];
        sodium_hex2bin(pubkey, crypto_box_PUBLICKEYBYTES, pubkey_hex.c_str(), pubkey_hex.size(), nullptr, nullptr, nullptr);

        users.push_back(User(clientSocket, firstMsg.sender, pubkey)); //data = pub_key
        std::cout << "User " << firstMsg.sender << " logged in." << std::endl;
    } //Zwolniony mutex
        
        std::string logMsg = "msg\n"; 
        logMsg += "Server\n";
        logMsg += firstMsg.sender;
        logMsg += '\n';
        logMsg += "Succesfully logged in";
        send_msg(clientSocket, "Server", Message(logMsg));
        logged = true;
        clientName = firstMsg.sender;
    }

    while(logged) {
        Message rcved = receive(clientSocket);

        if(rcved == Message::INVALID) {
                std::cerr << "Error receiving message" << std::endl;

                std::lock_guard<std::mutex> lock(usersMutex);
                int indx = receiver_in(clientName);
                users.erase(users.begin() + indx);
                closesocket(clientSocket);
                return;
            }

        if(rcved.type == "logout") { // "logout [ignore]"
            std::cout << "User " << clientName << " logged out." << std::endl;

            std::lock_guard<std::mutex> lock(usersMutex);
            int indx = receiver_in(clientName);
            users.erase(users.begin() + indx);
            logged = false;
            break;
        }

        if (rcved.type == "get_pubkey") {
            std::string target = rcved.receiver;
            int idx;
            {
            std::lock_guard<std::mutex> lock(usersMutex);
            idx = receiver_in(target);
            }

            if (idx == -1) {
                std::cerr << "No receiver found" << std::endl;
                Message reply("pubkey_error", "Server", clientName, target, "");
                send_msg(clientSocket, "Server", reply);
            } else {
                char hex[crypto_box_PUBLICKEYBYTES * 2 + 1];
                sodium_bin2hex(hex, sizeof(hex), users[idx].pub_key, crypto_box_PUBLICKEYBYTES);
                std::string hexkey = std::string(hex);
                Message reply("pubkey", "Server", clientName, target, hexkey);
                send_msg(clientSocket, "Server", reply);
            }
            continue;
        }

        if(rcved.type == "msg" && rcved.receiver == "") {
            continue;
        }

        send_msg(clientSocket, clientName, rcved);
    }

    closesocket(clientSocket);
}

Message Server::receive(SOCKET clientSocket) { 
    char buffer[1024] = {0};
    if(!recv_all(clientSocket, buffer, sizeof(buffer))) {
        return Message::INVALID;
    }
    int len = atoi(buffer);
    if(len <= 0 || len > 1000000) {
        std::cerr << "Invalid message" << std::endl;
        return Message::INVALID;
    }
    
    std::string msg;
    msg.resize(len);
    if (!recv_all(clientSocket, &msg[0], len)) {
        return Message::INVALID;
    }
    return Message(msg);
}

void Server::send_msg(SOCKET clientSocket, const std::string& sender, Message msg) {
    std::lock_guard<std::mutex> lock(usersMutex);
    if(msg == Message::INVALID) {
        std::cout << "Message is invalid" << std::endl;
        return;
    }
    if(receiver_in(msg.receiver) == -1) {
        Message user_msg = Message("msg", "Server", "", "User not found", "");
        std::string user_msg_str = std::string(user_msg);
        int len = user_msg_str.size();
        std::string tempLen = std::to_string(len);
        tempLen += std::string(1024 - tempLen.length(), ' ');
        send_all(clientSocket, tempLen.c_str(), 1024);
        send_all(clientSocket, user_msg_str.c_str(), len);
        return;
    }
    
    SOCKET receiverSocket = find_socket(msg.receiver);

    if(!sender.empty() && !msg.receiver.empty() && !msg.data.empty()) {
        msg.sender = sender;
        std::string strMsg = std::string(msg);
        int len = strMsg.length();
        std::string lenStr = std::to_string(len);
        lenStr += std::string(1024 - lenStr.length(), ' ');
        send_all(receiverSocket, lenStr.c_str(), lenStr.length());
        send_all(receiverSocket, strMsg.c_str(), len);
    }
}




int main() {
    if (sodium_init() < 0) {
        std::cerr << "sodium_init() failed" << std::endl;
        return 1;
    }

    Server server(8080);
    server.start();
    std::cout << "Server stopped." << std::endl;

    return 0;
}