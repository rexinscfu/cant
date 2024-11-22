#ifndef CANT_CRYPTO_UTILS_H
#define CANT_CRYPTO_UTILS_H

#include <stdint.h>
#include <stdbool.h>

// Cryptographic algorithm types
typedef enum {
    CRYPTO_ALG_XOR = 0,
    CRYPTO_ALG_AES128,
    CRYPTO_ALG_SHA256,
    CRYPTO_ALG_CUSTOM
} CryptoAlgorithm;

// Configuration for crypto operations
typedef struct {
    CryptoAlgorithm algorithm;
    uint8_t key[32];
    uint8_t key_length;
    bool use_hardware_acceleration;
    bool enable_secure_storage;
} CryptoConfig;

bool Crypto_Init(const CryptoConfig* config);
void Crypto_Deinit(void);

// Random number generation
bool Crypto_GenerateRandom(uint8_t* buffer, uint32_t length);
uint32_t Crypto_GenerateRandomU32(void);

// Key generation and validation
uint32_t Crypto_CalculateKey(uint32_t seed);
bool Crypto_ValidateKey(uint32_t seed, uint32_t key);

// Encryption/Decryption
bool Crypto_Encrypt(const uint8_t* input, uint32_t input_length,
                   uint8_t* output, uint32_t* output_length);
bool Crypto_Decrypt(const uint8_t* input, uint32_t input_length,
                   uint8_t* output, uint32_t* output_length);

// Hash functions
bool Crypto_CalculateHash(const uint8_t* data, uint32_t length,
                         uint8_t* hash, uint32_t hash_length);
bool Crypto_VerifyHash(const uint8_t* data, uint32_t length,
                      const uint8_t* expected_hash, uint32_t hash_length);

// Secure storage
bool Crypto_SecureStore(uint32_t id, const uint8_t* data, uint32_t length);
bool Crypto_SecureRetrieve(uint32_t id, uint8_t* data, uint32_t* length);

#endif // CANT_CRYPTO_UTILS_H 