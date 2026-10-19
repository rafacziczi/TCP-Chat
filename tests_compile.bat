@echo off
echo Kompilacja testow...
g++ -DSODIUM_STATIC -DRUNNING_TESTS -g tests.cpp client.cpp server.cpp googletest/src/gtest-all.cc -o run_tests.exe -Igoogletest/include -Igoogletest -Ilibsodium-win64/include -Llibsodium-win64/lib -lsodium -lws2_32 -lsqlite3 -lpthread

echo.
echo Compilation complete.
pause