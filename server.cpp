#include "header.h"

#pragma comment(lib, "Ws2_32.lib")

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
    std::tuple<std::string, std::string> receive(SOCKET);
    SOCKET find_socket(const std::string&);
    std::string find_name(const SOCKET);
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
        if(std::get<1>(user) == name) {
            return std::get<0>(user);
        }
    }

    return INVALID_SOCKET;
}

std::string Server::find_name(const SOCKET clientSocket) {
    for(const auto& user: users) {
        if(std::get<0>(user) == clientSocket) {
            return std::get<1>(user);
        }
    }

    return "";
}

int Server::receiver_in(const std::string& receiver) {
    for (int i = 0; i < users.size(); ++i) {
        if (std::get<1>(users[i]) == receiver) {
            return i;
        }
    }
    return -1;
}

void Server::handle_conn(SOCKET clientSocket) {
    bool logged = false;
    auto [receiver, message] = receive(clientSocket);
    std::string clientName;
    //Zakladamy: receiver = "login" lub receiver = "logout"

    if(receiver == "" && message == "") {
        std::cerr << "Error receiving message" << std::endl;
        if(logged) {
            std::lock_guard<std::mutex> lock(usersMutex);
            int indx = receiver_in(receiver);
            users.erase(users.begin() + indx);
        }
        closesocket(clientSocket);
        return;
    }

    if(receiver == "login") {
        {
        std::lock_guard<std::mutex> lock(usersMutex); //"login [nick] [ignore]"
        // Kazda kolejna wiadomosc zaczyna sie automatycznie od nicku nadawcy
        if(receiver_in(message) != -1) {
            std::cerr << "User " << message << " already logged in." << std::endl;
            closesocket(clientSocket);
            return;
        }

        users.push_back({clientSocket, message});
        std::cout << "User " << message << " logged in." << std::endl;
    } //Zwolniony mutex
        send_msg(clientSocket, "Server", message, "Logged successfully.");
        logged = true;
        clientName = message;
    }

    while(logged) {
        auto [receiver, message] = receive(clientSocket);

        if(receiver == "" && message == "") {
                std::cerr << "Error receiving message" << std::endl;

                std::lock_guard<std::mutex> lock(usersMutex);
                int indx = receiver_in(clientName);
                users.erase(users.begin() + indx);
                closesocket(clientSocket);
                return;
            }

        if(receiver == "logout") { // "logout [ignore]"
            std::cout << "User " << clientName << " logged out." << std::endl;

            std::lock_guard<std::mutex> lock(usersMutex);
            int indx = receiver_in(clientName);
            users.erase(users.begin() + indx);
            logged = false;
            break;
        }
        send_msg(clientSocket, clientName, receiver, message);
    }

    closesocket(clientSocket);
}

std::tuple<std::string, std::string> Server::receive(SOCKET clientSocket) { 
    char buffer[1024] = {0};
    std::string receiver, message;
    if(recv(clientSocket, buffer, sizeof(buffer), 0) == SOCKET_ERROR or buffer[0] == '\0') {
        return {"", ""};
    }
    int len = atoi(buffer);
    if(len > 0) {
        if(len > 1000000) {
            std::cerr << "Message too long" << std::endl;
            return {"", ""};
        }
        char* recv_msg = new char[len + 1]; // Sposob wiadomosci odebranej: [odbiorca] [wiadomosc]
        int rcved = recv(clientSocket, recv_msg, len, 0);
        if(rcved > 0) {
            recv_msg[rcved] = '\0';
        }

        std::string s(recv_msg);
        std::istringstream iss(s);
        iss >> receiver;
        std::getline(iss, message);
        if(!message.empty() && message[0] == ' '){
            message.erase(0, 1);
        }
        
        delete [] recv_msg;
    }
    return {receiver, message};
}

void Server::send_msg(SOCKET clientSocket, const std::string& sender, const std::string& receiver,
            const std::string& message) {
    std::lock_guard<std::mutex> lock(usersMutex);
    if(receiver_in(receiver) == -1) {
        std::string tempLen = "14";
        tempLen += std::string(1024 - tempLen.length(), ' ');
        send(clientSocket, tempLen.c_str(), 2, 0);
        send(clientSocket, "User not found", 14, 0);
        return;
    }

    SOCKET receiverSocket = find_socket(receiver);

    if(!sender.empty() && !receiver.empty() && !message.empty()) {
        int len = sender.length() + receiver.length() + message.length() + 2; // +2 spacje
        std::string lenStr = std::to_string(len);
        lenStr += std::string(1024 - lenStr.length(), ' ');
        send(receiverSocket, lenStr.c_str(), lenStr.length(), 0);
        std::string fullMessage = sender + " " + receiver + " " + message;
        send(receiverSocket, fullMessage.c_str(), len, 0);
    }
}






int main() {
    Server server(8080);
    server.start();
    std::cout << "Server stopped." << std::endl;

    return 0;
}