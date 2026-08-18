@echo off
g++ -DSODIUM_STATIC -g server.cpp -o server.exe -Ilibsodium-win64/include -Llibsodium-win64/lib -lsodium -lws2_32 -lsqlite3 -lpthread

g++ -DSODIUM_STATIC -g client.cpp -o client.exe -Ilibsodium-win64/include -Llibsodium-win64/lib -lsodium -lws2_32 -lsqlite3 -lpthread

echo.
echo Compilation complete.
pause