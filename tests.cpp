#include <gtest/gtest.h>
#include "header.h"
#include <sodium.h>
#include <sqlite3.h>
#include <string>

// --- DEKLARACJE FUNKCJI Z KLIENTA (client.cpp) ---
// Aby testy mogły z nich skorzystać, muszą zostać zadeklarowane lub przeniesione do wspólnego nagłówka.
extern bool check_keywords(const std::string& name);
extern std::string encrypt_msg(std::string plaintext, const unsigned char* reciver_pubkey, const unsigned char* my_priv_key);
extern std::string decrypt_msg(const std::string& ciphertext_hex, const unsigned char* sender_pubkey, const unsigned char* my_secret_key);

// =======================================================
// 1. TESTY STRUKTURY MESSAGE (header.h)
// =======================================================

// Testowanie konstruktora i konwersji do stringa (serializacja)
TEST(MessageTest, Serialization) {
    Message msg("msg", "Alice", "Bob", "HelloWorld", "pubkey123");
    std::string serialized = std::string(msg);
    EXPECT_EQ(serialized, "msg\nAlice\nBob\nHelloWorld\npubkey123");
}

// Testowanie konstruktora z argumentem string (deserializacja)
TEST(MessageTest, Deserialization) {
    std::string raw = "msg\nAlice\nBob\nHello\npubkey123";
    Message msg(raw);
    
    EXPECT_EQ(msg.type, "msg");
    EXPECT_EQ(msg.sender, "Alice");
    EXPECT_EQ(msg.receiver, "Bob");
    EXPECT_EQ(msg.data, "Hello");
    EXPECT_EQ(msg.pubkey, "pubkey123");
}

// Testowanie operatorów porównania
TEST(MessageTest, EqualityOperators) {
    Message msg1("msg", "A", "B", "Data", "Key");
    Message msg2("msg", "A", "B", "Data", "Key");
    Message msg3("login", "A", "B", "Data", "Key");
    
    EXPECT_TRUE(msg1 == msg2);
    EXPECT_TRUE(msg1 != msg3);
}

// =======================================================
// 2. TESTY WALIDACJI (client.cpp)
// =======================================================

TEST(ClientTest, CheckKeywordsReturnsTrueForReservedWords) {
    EXPECT_TRUE(check_keywords("register"));
    EXPECT_TRUE(check_keywords("login"));
    EXPECT_TRUE(check_keywords("logout"));
    EXPECT_TRUE(check_keywords("msg"));
    EXPECT_TRUE(check_keywords("Server"));
}

TEST(ClientTest, CheckKeywordsReturnsFalseForValidNicks) {
    EXPECT_FALSE(check_keywords("Alice"));
    EXPECT_FALSE(check_keywords("JohnDoe_99"));
}

// =======================================================
// 3. TESTY KRYPTOGRAFICZNE LIBSODIUM (client.cpp)
// =======================================================

class CryptoTest : public ::testing::Test {
protected:
    unsigned char alice_pub[crypto_box_PUBLICKEYBYTES];
    unsigned char alice_priv[crypto_box_SECRETKEYBYTES];
    unsigned char bob_pub[crypto_box_PUBLICKEYBYTES];
    unsigned char bob_priv[crypto_box_SECRETKEYBYTES];

    void SetUp() override {
        // Inicjalizacja biblioteki w każdym teście
        ASSERT_GE(sodium_init(), 0);
        crypto_box_keypair(alice_pub, alice_priv);
        crypto_box_keypair(bob_pub, bob_priv);
    }
};

TEST_F(CryptoTest, EncryptAndDecryptSuccess) {
    std::string plaintext = "Tajny komunikat testowy";
    
    // Szyfrowanie wiadomości przez Alice dla Boba
    std::string ciphertext_hex = encrypt_msg(plaintext, bob_pub, alice_priv);
    EXPECT_FALSE(ciphertext_hex.empty());
    EXPECT_NE(ciphertext_hex, plaintext);
    
    // Deszyfrowanie wiadomości przez Boba
    std::string decrypted = decrypt_msg(ciphertext_hex, alice_pub, bob_priv);
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(CryptoTest, DecryptionFailsWithWrongKey) {
    std::string plaintext = "Tajny komunikat testowy";
    std::string ciphertext_hex = encrypt_msg(plaintext, bob_pub, alice_priv);
    
    // Trzecia osoba (Eve) próbuje odszyfrować używając swojego klucza
    unsigned char eve_pub[crypto_box_PUBLICKEYBYTES];
    unsigned char eve_priv[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair(eve_pub, eve_priv);
    
    std::string decrypted = decrypt_msg(ciphertext_hex, alice_pub, eve_priv);
    
    // Funkcja powinna zwrócić pusty string, jeśli macierz MAC / klucz się nie zgadza
    EXPECT_TRUE(decrypted.empty());
}

TEST_F(CryptoTest, DecryptionFailsWithCorruptedCiphertext) {
    std::string plaintext = "Test";
    std::string ciphertext_hex = encrypt_msg(plaintext, bob_pub, alice_priv);
    
    // Uszkodzenie szyfrogramu
    ciphertext_hex[5] = ciphertext_hex[5] == 'a' ? 'b' : 'a'; 
    
    std::string decrypted = decrypt_msg(ciphertext_hex, alice_pub, bob_priv);
    EXPECT_TRUE(decrypted.empty());
}

// =======================================================
// 4. TESTY BAZY DANYCH (server.cpp)
// =======================================================

// Test weryfikujący poprawność schematu i zapytań SQLites
TEST(ServerDBTest, UserRegistrationAndLoadingFromMemoryDB) {
    sqlite3* db;
    // Tworzymy bazę danych w pamięci RAM wyłącznie na potrzeby testu
    ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

    // Symulacja inicjalizacji schematu z funkcji main() w server.cpp
    const char* schema_sql = "CREATE TABLE IF NOT EXISTS users ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                             "nickname TEXT NOT NULL UNIQUE, "
                             "public_key TEXT NOT NULL, "
                             "password_hash TEXT NOT NULL);";
    char* errMsg = 0;
    ASSERT_EQ(sqlite3_exec(db, schema_sql, 0, 0, &errMsg), SQLITE_OK);

    // Symulacja rejestracji (Server::registerUser)
    const char* insert_sql = "INSERT INTO users (nickname, public_key, password_hash) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    ASSERT_EQ(sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL), SQLITE_OK);
    sqlite3_bind_text(stmt, 1, "TestUser", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, "abcd1234pubkeyhex", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, "hashed_password_123", -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_DONE);
    sqlite3_finalize(stmt);

    // Weryfikacja działania odczytu (Server::loadUsersDB)
    const char* select_sql = "SELECT public_key, password_hash FROM users WHERE nickname = 'TestUser';";
    ASSERT_EQ(sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), "abcd1234pubkeyhex");
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)), "hashed_password_123");
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

// =======================================================
// ENTRY POINT DLA GOOGLE TEST
// =======================================================
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}