#include "header.h"

std::mutex usersMutex;

std::vector<std::tuple<SOCKET, std::string>> users;



int receiver_in(const std::string& receiver) {
    std::lock_guard<std::mutex> lock(usersMutex);

    for (int i = 0; i < users.size(); ++i) {
        if (std::get<1>(users[i]) == receiver) {
            return i;
        }
    }
    return -1;
}

void handle_conn(SOCKET clientSocket) {
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

std::tuple<std::string, std::string, std::string> receive(SOCKET clientSocket) { 
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

void send_msg(SOCKET clientSocket, const std::string& sender, const std::string& receiver,
            const std::string& message) {
    if(receiver_in(receiver) == -1) {
        return;
    }
    if(!sender.empty() && !receiver.empty() && !message.empty()) {
        int len = sender.length() + receiver.length() + message.length() + 2; // +2 spaces
        send(clientSocket, std::to_string(len).c_str(), len, 0);
        std::string fullMessage = sender + " " + receiver + " " + message;
        send(clientSocket, fullMessage.c_str(), len, 0);
    }
}


int main() {
    WSADATA wsaData;


    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)
        ) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        return 1;
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
            std::thread clientThread(handle_conn, clientSocket);
            clientThread.detach();
        }
        else {
            std::cerr << "Accept failed" << std::endl;
        }
    }


    return 0;
}