# ChatTCP - Secure E2EE Communicator

A multithreaded network messenger based on a client-server architecture using TCP sockets (Winsock). The application places a strong emphasis on security—implementing full End-to-End Encryption (E2EE) for transmitted messages and secure storage of authentication credentials.

## Key Features

- **Client-Server Architecture:** Asynchronous handling of multiple concurrent clients using threads (`std::thread`) and synchronization mechanisms (`std::mutex`, `std::condition_variable`).
- **End-to-End Encryption (E2EE):** Powered by the `libsodium` library for secure communication. Messages are encrypted locally on the sender's device using the recipient's public key (`crypto_box_easy`) and can only be decrypted by the intended recipient. The server acts strictly as a zero-knowledge relay for the encrypted payload.
- **Password Security:** User passwords are never stored in plaintext. The modern Argon2id hashing algorithm (`crypto_pwhash`), integrated via `libsodium`, is used for credential storage.
- **Persistent Database:** The server utilizes an embedded SQLite3 database to persistently store user IDs, password hashes, and public keys.
- **Test Coverage:** Business logic, data structures, and cryptographic algorithms are thoroughly tested using the Google Test (GTest) framework.

## Requirements and Dependencies

The project is designed for the Windows environment. The following are required to compile and run the application:

- A C++ compiler supporting the **C++17** standard.
- **libsodium** library (encryption).
- **SQLite3** library (database).
- **Google Test** framework (for running unit tests).
- Linking against the system library `ws2_32.lib` (Windows Sockets 2).

## Compilation

The project includes a ready-to-use batch script (`.bat`) for the `g++` compiler (MinGW). The script compiles the server, the client, and the unit test suite, linking the GTest framework on the fly.

```bat
@echo off
echo Kompilacja serwera...
g++ -DSODIUM_STATIC -g server.cpp -o server.exe -Ilibsodium-win64/include -Llibsodium-win64/lib -lsodium -lws2_32 -lsqlite3 -lpthread

echo Kompilacja klienta...
g++ -DSODIUM_STATIC -g client.cpp -o client.exe -Ilibsodium-win64/include -Llibsodium-win64/lib -lsodium -lws2_32 -lsqlite3 -lpthread

echo Kompilacja testow...
g++ -DSODIUM_STATIC -DRUNNING_TESTS -g tests.cpp client.cpp server.cpp googletest/src/gtest-all.cc -o run_tests.exe -Igoogletest/include -Igoogletest -Ilibsodium-win64/include -Llibsodium-win64/lib -lsodium -lws2_32 -lsqlite3 -lpthread
```

## Usage

1. **Starting the server:** Always launch the server first. It will generate the database file (`users.db`) if it does not already exist and begin listening on port 8080.
   ```cmd
   server.exe
   ```

2. **Starting the client:** Run one or more client instances in separate terminal windows.
   ```cmd
   client.exe
   ```

### Available Client Commands

- `register [username] [password]` – Registers a new user. Generates a key pair and saves the private key locally in `%APPDATA%\ChatTCP\`.
- `login [username] [password]` – Logs in an existing user.
- `msg [recipient_username] [message_body]` – Sends an encrypted private message to the specified user. The client automatically fetches the recipient's public key from the server.
- `logout` – Logs out and safely closes the application.

## Testing

To run the unit tests validating data structures, validation rules, and libsodium cryptographic functions, execute the compiled test binary:

```cmd
run_tests.exe
```

## Project Structure

- `server.cpp` – Main server loop, socket handling, database integration, and connected user management.
- `client.cpp` – User console application featuring asynchronous message reception alongside encryption and decryption.
- `header.h` – Shared data structures (such as the `Message` class) used for parsing network packets.
- `tests.cpp` – Comprehensive unit test suite (GTest) covering core architectural components.