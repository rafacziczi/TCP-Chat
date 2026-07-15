#include "header.h"

#pragma comment(lib, "Ws2_32.lib") 


const std::string SERVER_IP = "127.0.0.1";
const std::string keywords[] = {"login", "logout", "msg"};
std::mutex clientMutex;

bool running = true;

bool check_keywords(const std::string& name) {
    for(const auto& word: keywords) {
        if(name == word) {
            return true;
        }
    }
    return false;
}

void send_client(const std::string& command, const std::string& receiver, const std::string& message, SOCKET clientSocket, 
    std::string& client_name) {
    std::lock_guard<std::mutex> lock(clientMutex);
    std::string msg;
    int len = 0;
    if(command == "login") {
        if(check_keywords(receiver)) {
            std::cout << "Cannot use reserved keywords as nicknames." << std::endl;
            return;
        }
        else if(client_name != "") {
            std::cout << "You are already logged in as " << client_name << "." << std::endl;
            return;
        }
        else if(!check_keywords(receiver) && receiver != "") {
            client_name = receiver;
            msg = command + " " + receiver;
        }
    }
    else if(command == "logout") {
        if(client_name == "") {
            std::cout << "You are not logged in." << std::endl;
            return;
        }
        else {
            msg = command;
            client_name = "";
            running = false; //Zatrzymanie clienta
            std::cout << "Disconnected" << std::endl;
        }
    }
    else if(command == "msg") {
        if(client_name == "") {
            std::cout << "You are not logged in." << std::endl;
            return;
        }
        else if(receiver == "") {
            std::cout << "No receiver specified." << std::endl;
            return;
        }
        else if(message == "") {
            std::cout << "No message specified." << std::endl;
            return;
        }
        else {
            msg = receiver + " " + message;
        }
    }
    else {
        std::cout << "Unknown command." << std::endl;
        return;
    }

    len = msg.length();
    std::string len_str = std::to_string(len);
    len_str += std::string(1024 - len_str.length(), ' ');
    send(clientSocket, len_str.c_str(), 1024, 0);
    send(clientSocket, msg.c_str(), len, 0);
}


void receive_client(SOCKET clientSocket) {
    char buffer[1024] = {0};
    std::string sender, receiver, message;

    while(running) {
        memset(buffer, 0, sizeof(buffer));

        if(recv(clientSocket, buffer, sizeof(buffer), 0) == SOCKET_ERROR or buffer[0] == '\0') {
            return;
        }
        int len = atoi(buffer);

        if(len > 1000000) {
                continue;
            }

        if(len > 0) {
            char* recv_msg = new char[len + 1]; // Sposob wiadomosci: [nadawca] [odbiorca] [wiadomosc]
            int rcved = recv(clientSocket, recv_msg, len, 0);
            if(rcved > 0) {
                recv_msg[rcved] = '\0';
            }
            else {
                delete[] recv_msg;
                break;
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
        std::cout << sender << ": " << message << std::endl;
        std::cout << ">";
        std::cout.flush();
    }
}

int main() {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "socket() failed" << std::endl;
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);

    inet_pton(AF_INET, SERVER_IP.c_str(), &serverAddr.sin_addr);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR){
        std::cerr << "Connection failed" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected!" << std::endl;
    std::cout << "Enter 'login' to log in," << std::endl;
    std::cout << "Enter 'logout' to log out," << std::endl;
    std::cout << "Enter 'msg [nick] [message]' to send a message." << std::endl; 

    std::thread receiverThread(receive_client, clientSocket);

    std::string input;
    std::string client_name = "";
    std::stringstream ss;
    std::string command, receiver, message;
    while(running) {
        std::cout << ">";
        input = "";
        getline(std::cin, input);
        ss.clear();
        ss.str(input);
        ss >> command >> receiver;
        std::getline(ss>>std::ws, message);

        send_client(command, receiver, message, clientSocket, client_name);
    }

    closesocket(clientSocket);

    if (receiverThread.joinable()) {
        receiverThread.join();
    }

    WSACleanup();
    std::cout << "Closed" << std::endl;

    return 0;
}