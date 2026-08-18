#include "header.h"


const std::string SERVER_IP = "127.0.0.1";
const std::string keywords[] = {"register", "login", "logout", "msg", "Server"};
std::mutex clientMutex;
unsigned char my_pub_key[crypto_box_PUBLICKEYBYTES];
unsigned char my_priv_key[crypto_box_SECRETKEYBYTES];

std::mutex pubkeyMutex;
std::condition_variable pubkeyCv;
std::unordered_map<std::string, std::string> pendingPubkeys; //klucz: nick


bool running = true;

bool check_keywords(const std::string& name) {
    for(const auto& word: keywords) {
        if(name == word) {
            return true;
        }
    }
    return false;
}

std::string decrypt_msg(const std::string& ciphertext_hex, const unsigned char* sender_pubkey,
    const unsigned char* my_secret_key) {
    size_t bin_len = ciphertext_hex.size() / 2;
    std::vector<unsigned char> combined(bin_len);
    if (sodium_hex2bin(combined.data(), combined.size(), ciphertext_hex.c_str(), ciphertext_hex.size(),
        nullptr, nullptr, nullptr) != 0) {
        return "";
    }
    
    if (combined.size() < crypto_box_NONCEBYTES + crypto_box_MACBYTES) {
        return "";
    }
    
    unsigned char nonce[crypto_box_NONCEBYTES];
    memcpy(nonce, combined.data(), crypto_box_NONCEBYTES);
    const unsigned char* ciphertext = combined.data() + crypto_box_NONCEBYTES;
    size_t ciphertext_len = combined.size() - crypto_box_NONCEBYTES;
    
    std::vector<unsigned char> decrypted(ciphertext_len - crypto_box_MACBYTES);
    
    int result = crypto_box_open_easy(decrypted.data(), ciphertext, ciphertext_len, nonce,
    sender_pubkey, my_secret_key);
    if (result != 0) {
        return "";
    }
    
    return std::string((char*)decrypted.data(), decrypted.size());
}


std::string encrypt_msg(std::string plaintext, const unsigned char* reciver_pubkey, const unsigned char* my_priv_key) {
    unsigned char nonce[crypto_box_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));
    
    std::vector<unsigned char> ciphertext(plaintext.size() + crypto_box_MACBYTES);
    
    int result = crypto_box_easy(ciphertext.data(), (const unsigned char*)plaintext.c_str(), plaintext.size(), nonce,
    reciver_pubkey, my_priv_key);
    if (result != 0) {
        return "";
    }
    
    std::vector<unsigned char> combined;
    combined.reserve(crypto_box_NONCEBYTES + ciphertext.size());
    combined.insert(combined.end(), nonce, nonce + crypto_box_NONCEBYTES);
    combined.insert(combined.end(), ciphertext.begin(), ciphertext.end());
    
    char hex_output[combined.size() * 2 + 1];
    sodium_bin2hex(hex_output, sizeof(hex_output), combined.data(), combined.size());
    if (hex_output == nullptr) {
        return "";
    }
    std::string result_hex(hex_output);
    
    return result_hex;
}

Message receive_client(SOCKET);

void send_client(const std::string& command, const std::string& receiver, const std::string& message, SOCKET clientSocket, 
    std::string& client_name) {
    std::lock_guard<std::mutex> lock(clientMutex);

    char temp_hex[crypto_box_PUBLICKEYBYTES * 2 + 1];
    sodium_bin2hex(temp_hex, sizeof(temp_hex), my_pub_key, crypto_box_PUBLICKEYBYTES);
    std::string pubkey_str(temp_hex);

    Message msg = Message(command, "", receiver, message, pubkey_str);
    unsigned char recv_pub_key[crypto_box_PUBLICKEYBYTES];
    int len = 0;
    if(msg.type == "register") {
        msg.sender = receiver;
        if(check_keywords(msg.sender)) {
            std::cout << "Cannot use reserved keywords as nicknames." << std::endl;
            return;
        }
        else if(client_name != "") {
            std::cout << "You are already logged in as " << client_name << "." << std::endl;
            return;
        }
        else if(!check_keywords(msg.sender) && msg.sender != "") {
            char hex[crypto_box_PUBLICKEYBYTES * 2 + 1];
            sodium_bin2hex(hex, sizeof(hex), my_pub_key, crypto_box_PUBLICKEYBYTES);
            msg.pubkey = hex;
        }

        if(msg.data == "") {
            std::cout << "No password specified." << std::endl;
            return;
        }
        client_name = receiver;
    }
    else if(msg.type == "login") {
        msg.sender = receiver;
        if(check_keywords(msg.sender)) {
            std::cout << "Cannot use reserved keywords as nicknames." << std::endl;
            return;
        }
        else if(client_name != "") {
            std::cout << "You are already logged in as " << client_name << "." << std::endl;
            return;
        }
        else if(!check_keywords(msg.sender) && msg.sender != "") {
            char hex[crypto_box_PUBLICKEYBYTES * 2 + 1];
            sodium_bin2hex(hex, sizeof(hex), my_pub_key, crypto_box_PUBLICKEYBYTES);
            msg.pubkey = hex;
        }

        if(msg.data == "") {
            std::cout << "No password specified." << std::endl;
            return;
        }
        client_name = receiver; //NAPRAWIC aby bylo dopiero po potwierdzeniu logowania
    }
    else if(msg.type == "logout") {
        if(client_name == "") {
            std::cout << "You are not logged in." << std::endl;
            return;
        }
        else {
            client_name = "";
            running = false; //Zatrzymanie clienta
            std::cout << "Disconnected" << std::endl;
        }
    }
    else if(msg.type == "msg") {
        if(client_name == "") {
            std::cout << "You are not logged in." << std::endl;
            return;
        }
        else if(msg.receiver == "") {
            std::cout << "No receiver specified." << std::endl;
            return;
        }
        else if(msg.data == "") {
            std::cout << "No message specified." << std::endl;
            return;
        }
        Message get_pubkey = Message("get_pubkey", "", msg.receiver, "", "");
        std::string get_str = std::string(get_pubkey);
        int get_len = get_str.length();
        std::string get_len_str = std::to_string(get_len);
        get_len_str += std::string(1024 - get_len_str.length(), ' ');
        send_all(clientSocket, get_len_str.c_str(), 1024);
        send_all(clientSocket, get_str.c_str(), get_len);

        std::string pubkeyStr;

        {
            std::unique_lock<std::mutex> lock(pubkeyMutex);

            pubkeyCv.wait(lock, [&]{
                return pendingPubkeys.count(msg.receiver);
            });

            pubkeyStr = pendingPubkeys[msg.receiver];
            pendingPubkeys.erase(msg.receiver);
        }
        if(pubkeyStr.empty()) {
            std::cout << "Public key is empty!" << std::endl;
            msg.data = "";
        }
        else{
            sodium_hex2bin(recv_pub_key, crypto_box_PUBLICKEYBYTES, pubkeyStr.c_str(), pubkeyStr.size(), nullptr, nullptr, nullptr);
            std::string plaindata = msg.data;
            msg.data = encrypt_msg(plaindata, recv_pub_key, my_priv_key);
        }
        }
    else if(msg.type == "get_pubkey") {
        if(receiver == "") {
            std::cout << "No receiver specified" << std::endl;
        }
    }
    else {
        std::cout << "Unknown command." << std::endl;
        return;
    }

    std::string send_msg = std::string(msg);
    len = send_msg.length();
    std::string len_str = std::to_string(len);
    len_str += std::string(1024 - len_str.length(), ' ');
    send_all(clientSocket, len_str.c_str(), 1024);
    send_all(clientSocket, send_msg.c_str(), len);
}


Message receive_client(SOCKET clientSocket) {
    char buffer[1024] = {0};
    if(!recv_all(clientSocket, buffer, sizeof(buffer))) {
        return Message::INVALID;
    }
    int len = atoi(buffer);
    if(len <= 0 || len > 1000000) {
        std::cerr << "Invalid message" << std::endl;
    }
    
    std::string msg;
    msg.resize(len);
    if (!recv_all(clientSocket, &msg[0], len)) {
        return Message::INVALID;
    }

    Message msg_rcved = Message(msg);

    if(msg_rcved.type == "pubkey_error") {
        {
            std::lock_guard<std::mutex> lock(pubkeyMutex);
            pendingPubkeys[msg_rcved.data] = "";
        }

        pubkeyCv.notify_all();
        return Message();
    }

    if(msg_rcved.type == "pubkey") {
        {
        std::lock_guard<std::mutex> lock(pubkeyMutex);
        pendingPubkeys[msg_rcved.data] = msg_rcved.pubkey;
        }

        pubkeyCv.notify_all();

        return Message();
    }

    if(msg_rcved.type == "msg" && msg_rcved.sender != "Server") {
        std::string pubkeyString = msg_rcved.pubkey;
        unsigned char send_pubkey[crypto_box_PUBLICKEYBYTES];
        sodium_hex2bin(send_pubkey, crypto_box_PUBLICKEYBYTES, pubkeyString.c_str(), pubkeyString.size(), nullptr, nullptr, nullptr);
        std::string ciphertext = msg_rcved.data;
        std::string deciphertext = decrypt_msg(ciphertext, send_pubkey, my_priv_key);
        msg_rcved.data = deciphertext;
    }

    return msg_rcved;
}

void receive_loop(SOCKET clientSocket) {
    Message msg_rcved;

    while(running) {
        msg_rcved = receive_client(clientSocket);

        if(msg_rcved.type == "msg") {
            std::cout << msg_rcved.sender << ": " << msg_rcved.data << std::endl;
            std::cout << ">";
            std::cout.flush();
        }
    }
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "sodium_init() failed" << std::endl;
        return 1;
    }

    crypto_box_keypair(my_pub_key, my_priv_key);

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
    std::cout << "Enter 'register' to register," << std::endl;
    std::cout << "Enter 'login' to log in," << std::endl;
    std::cout << "Enter 'logout' to log out," << std::endl;
    std::cout << "Enter 'msg [nick] [message]' to send a message." << std::endl; 

    std::thread receiverThread(receive_loop, clientSocket);

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