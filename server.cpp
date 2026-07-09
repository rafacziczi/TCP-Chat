#include "header.h"

class Server {
private:
    SOCKET serverSocket;
    sockaddr_in serverAddress;
    std::vector<std::tuple<SOCKET, std::string>> users;
    std::mutex usersMutex;
public:
    Server(int port);
    ~Server();
    void start();
    void send_msg(SOCKET, const std::string&, const std::string&, const std::string&);
    int receiver_in(const std::string&);
    void handle_conn(SOCKET);
    std::tuple<std::string, std::string, std::string> receive(SOCKET);
    SOCKET find_name(const std::string&);
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

    
    
    listen(serverSocket, SOMAXCONN);

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

SOCKET Server::find_name(const std::string& name) {
    std::lock_guard<std::mutex> lock(usersMutex);
    for(const auto& user : users) {
        if(std::get<1>(user) == name) {
            return std::get<0>(user);
        }
    }

    return INVALID_SOCKET;
}

int Server::receiver_in(const std::string& receiver) {
    std::lock_guard<std::mutex> lock(usersMutex);

    for (int i = 0; i < users.size(); ++i) {
        if (std::get<1>(users[i]) == receiver) {
            return i;
        }
    }
    return -1;
}

void Server::handle_conn(SOCKET clientSocket) {
    auto [sender, receiver, message] = receive(clientSocket);
    //Zakladamy: sender = "login" lub sender = "logout"

    if(sender == "login") {
        if(receiver_in(receiver) != -1) {
            std::cerr << "User " << receiver << " already logged in." << std::endl;
            closesocket(clientSocket);
            return;
        }

        std::lock_guard<std::mutex> lock(usersMutex);
        users.push_back({clientSocket, receiver});
        std::cout << "User " << receiver << " logged in." << std::endl;
    }

    while(true) {
        auto [sender, receiver, message] = receive(clientSocket);
        if(sender == "logout") {
            std::cout << "User " << receiver << " logged out." << std::endl;

            std::lock_guard<std::mutex> lock(usersMutex);
            users.erase(users.begin() + receiver_in(receiver));
            break;
        }
        send_msg(clientSocket, sender, receiver, message);
    }

    closesocket(clientSocket);
}

std::tuple<std::string, std::string, std::string> Server::receive(SOCKET clientSocket) { 
    char buffer[1024] = {0};
    std::string sender, receiver, message;
        recv(clientSocket, buffer, sizeof(buffer), 0);
        int len = atoi(buffer);
        if(len > 0) {
            char* recv_msg = new char[len + 1]; // Sposob wiadomosci: [nadawca] [odbiorca] [wiadomosc]
            int rcved = recv(clientSocket, recv_msg, len, 0);
            if(rcved > 0) {
                recv_msg[rcved] = '\0';
            }

            std::string s(recv_msg);
            std::istringstream iss(s);
            iss >> sender >> receiver;
            std::getline(iss, message);
            if(!message.empty() && message[0] == ' '){
                message.erase(0, 1);
            }
            
            delete [] recv_msg;
        }
    return {sender, receiver, message};
}

void Server::send_msg(SOCKET clientSocket, const std::string& sender, const std::string& receiver,
            const std::string& message) {
    if(receiver_in(receiver) == -1) {
        send(clientSocket, "User not found", 14, 0);
        return;
    }

    SOCKET receiverSocket = find_name(receiver);

    if(!sender.empty() && !receiver.empty() && !message.empty()) {
        int len = sender.length() + receiver.length() + message.length() + 2; // +2 spaces
        std::string lenStr = std::to_string(len);
        send(receiverSocket, lenStr.c_str(), lenStr.length(), 0);
        std::string fullMessage = sender + " " + receiver + " " + message;
        send(receiverSocket, fullMessage.c_str(), len, 0);
    }
}


int main() {
    Server server(8080);
    server.start();

    return 0;
}