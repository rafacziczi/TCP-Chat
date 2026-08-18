#ifndef HEADER_H
#define HEADER_H

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libsodium.lib")
#pragma comment(lib, "sqlite3.lib")

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdlib>
#include <string>
#include <sstream>
#include <tuple>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include "libsodium-win64/include/sodium.h"
#include "sqlite3.h"


struct Message {
    std::string type;
    std::string sender;
    std::string receiver;
    std::string data;
    std::string pubkey;

    static const Message INVALID;


    operator std::string() const {
        return type + '\n' + sender + '\n' + receiver + '\n' + data + '\n' + pubkey;
    }

    bool operator==(const Message& other) const {
        return type == other.type && 
            sender == other.sender &&
            receiver == other.receiver && 
            data == other.data;
        }

    bool operator!=(const Message& other) const {
        return !(*this == other);
    }

    Message() {}

    Message(const std::string& t, const std::string& s, const std::string& r, const std::string& d, const std::string& p)
    : type(t), sender(s), receiver(r), data(d), pubkey(p) {}

    Message(std::string message) { 
        int i = 0;
        type = "";
        receiver = "";
        sender = "";
        data = "";
        pubkey = "";

        while(i < message.length() && message[i] != '\n') {
            type += message[i];
            ++i;
        }
        ++i;

         while(i < message.length() && message[i] != '\n') {
            sender += message[i];
            ++i;
        }
        ++i;

        while(i < message.length() && message[i] != '\n') {
            receiver += message[i];
            ++i;
        }
        ++i;

        while(i < message.length() && message[i] != '\n') {
            data += message[i];
            ++i;
        }
        ++i;

        while(i < message.length()) {
            pubkey += message[i];
            ++i;
        }
    }
};

inline const Message Message::INVALID{"INVALID_MESSAGE", "", "", "", ""};



bool send_all(SOCKET s, const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int result = send(s, data + sent, len - sent, 0);
        if (result == SOCKET_ERROR) return false;
        sent += result;
    }
    return true;
}

bool recv_all(SOCKET s, char* buffer, int len) {
    int received = 0;
    while (received < len) {
        int result = recv(s, buffer + received, len - received, 0);
        if (result <= 0) return false; // błąd lub zamknięcie
        received += result;
    }
    return true;
}

#endif