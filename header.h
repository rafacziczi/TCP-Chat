#ifndef HEADER_H
#define HEADER_H

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

void send_msg(SOCKET, const std::string&, const std::string&, const std::string&);

int receiver_in(const std::string&);

void handle_conn(SOCKET);

std::tuple<std::string, std::string, std::string> receive(SOCKET);

#endif