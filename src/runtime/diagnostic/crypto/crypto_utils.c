#include "crypto_utils.h"
#include "../logging/diag_logger.h"
#include "../os/critical.h"
#include <string.h>
#include <time.h>

#define SECURE_STORAGE_SIZE (64 * 1024)  // 64KB secure storage
#define MAX_SECURE_ENTRIES 256

typedef struct {
    uint32_t id;
    uint32_t length;
    uint32_t offset;
    uint8_t hash[32];
} SecureEntry;

typedef struct {
    CryptoConfig config;
    uint8_t secure_storage[SECURE_STORAGE_SIZE];
    SecureEntry entries[MAX_SECURE_ENTRIES];
    uint32_t entry_count;
    bool initialized;
} CryptoContext;

static CryptoContext crypto_ctx;

static uint32_t software_random_seed = 0;

static void init_software_random(void) {
    software_random_seed = (uint32_t)time(NULL);
}

static uint32_t generate_software_random(void) {
    software_random_seed = software_random_seed * 1103515245 + 12345;
    return software_random_seed;
}

static bool find_secure_entry(uint32_t id, SecureEntry** entry) {
    for (uint32_t i = 0; i < crypto_ctx.entry_count; i++) {
        if (crypto_ctx.entries[i].id == id) {
            *entry = &crypto_ctx.entries[i];
            return true;
        }
    }
    return false;
}

bool Crypto_Init(const CryptoConfig* config) {
    if (!config) {
        return false;
    }

    memset(&crypto_ctx, 0, sizeof(CryptoContext));
    memcpy(&crypto_ctx.config, config, sizeof(CryptoConfig));

    if (config->use_hardware_acceleration) {
        if (!HW_Crypto_Init()) {
            Logger_Log(LOG_LEVEL_ERROR, "CRYPTO", "Hardware crypto initialization failed");
            return false;
        }
    } else {
        init_software_random();
    }

    crypto_ctx.initialized = true;
    Logger_Log(LOG_LEVEL_INFO, "CRYPTO", "Crypto system initialized");
    return true;
}

void Crypto_Deinit(void) {
    if (crypto_ctx.config.use_hardware_acceleration) {
        HW_Crypto_Deinit();
    }
    
    // Securely clear sensitive data
    memset(&crypto_ctx, 0, sizeof(CryptoContext));
    Logger_Log(LOG_LEVEL_INFO, "CRYPTO", "Crypto system deinitialized");
}

bool Crypto_GenerateRandom(uint8_t* buffer, uint32_t length) {
    if (!crypto_ctx.initialized || !buffer || length == 0) {
        return false;
    }

    if (crypto_ctx.config.use_hardware_acceleration) {
        return HW_Crypto_GenerateRandom(buffer, length);
    }

    for (uint32_t i = 0; i < length; i++) {
        buffer[i] = (uint8_t)(generate_software_random() & 0xFF);
    }
    return true;
}

uint32_t Crypto_GenerateRandomU32(void) {
    uint32_t value = 0;
    Crypto_GenerateRandom((uint8_t*)&value, sizeof(value));
    return value;
}

uint32_t Crypto_CalculateKey(uint32_t seed) {
    uint32_t key = seed;
    
    switch (crypto_ctx.config.algorithm) {
        case CRYPTO_ALG_XOR:
            key ^= 0x55AA55AA;
            break;
            
        case CRYPTO_ALG_AES128: {
            uint8_t input[16] = {0};
            uint8_t output[16] = {0};
            memcpy(input, &seed, sizeof(seed));
            
            if (crypto_ctx.config.use_hardware_acceleration) {
                HW_Crypto_AES128_Encrypt(input, sizeof(input), output, crypto_ctx.config.key);
            } else {
                // Software AES implementation would go here
                // For now, use a simple transformation
                for (int i = 0; i < 16; i++) {
                    output[i] = input[i] ^ crypto_ctx.config.key[i];
                }
            }
            memcpy(&key, output, sizeof(key));
            break;
        }
            
        case CRYPTO_ALG_SHA256: {
            uint8_t hash[32] = {0};
            if (crypto_ctx.config.use_hardware_acceleration) {
                HW_Crypto_SHA256((uint8_t*)&seed, sizeof(seed), hash);
            } else {
                // Software SHA-256 implementation would go here
                // For now, use a simple hash
                for (int i = 0; i < 32; i++) {
                    hash[i] = ((uint8_t*)&seed)[i % 4] ^ crypto_ctx.config.key[i % 16];
                }
            }
            memcpy(&key, hash, sizeof(key));
            break;
        }
            
        case CRYPTO_ALG_CUSTOM:
            // Custom algorithm implementation
            key = (seed * 0x8088405) ^ 0x12345678;
            break;
    }
    
    return key;
}

bool Crypto_ValidateKey(uint32_t seed, uint32_t key) {
    return Crypto_CalculateKey(seed) == key;
}

bool Crypto_Encrypt(const uint8_t* input, uint32_t input_length,
                   uint8_t* output, uint32_t* output_length) {
    if (!crypto_ctx.initialized || !input || !output || !output_length) {
        return false;
    }

    if (crypto_ctx.config.algorithm == CRYPTO_ALG_AES128) {
        if (input_length % 16 != 0) {
            Logger_Log(LOG_LEVEL_ERROR, "CRYPTO", "Input length must be multiple of 16 for AES");
            return false;
        }

        *output_length = input_length;
        
        if (crypto_ctx.config.use_hardware_acceleration) {
            return HW_Crypto_AES128_Encrypt(input, input_length, output, crypto_ctx.config.key);
        } else {
            // Software encryption
            for (uint32_t i = 0; i < input_length; i++) {
                output[i] = input[i] ^ crypto_ctx.config.key[i % crypto_ctx.config.key_length];
            }
            return true;
        }
    }

    return false;
}

bool Crypto_Decrypt(const uint8_t* input, uint32_t input_length,
                   uint8_t* output, uint32_t* output_length) {
    if (!crypto_ctx.initialized || !input || !output || !output_length) {
        return false;
    }

    if (crypto_ctx.config.algorithm == CRYPTO_ALG_AES128) {
        if (input_length % 16 != 0) {
            Logger_Log(LOG_LEVEL_ERROR, "CRYPTO", "Input length must be multiple of 16 for AES");
            return false;
        }

        *output_length = input_length;
        
        if (crypto_ctx.config.use_hardware_acceleration) {
            return HW_Crypto_AES128_Decrypt(input, input_length, output, crypto_ctx.config.key);
        } else {
            // Software decryption (XOR is its own inverse)
            for (uint32_t i = 0; i < input_length; i++) {
                output[i] = input[i] ^ crypto_ctx.config.key[i % crypto_ctx.config.key_length];
            }
            return true;
        }
    }

    return false;
}

bool Crypto_CalculateHash(const uint8_t* data, uint32_t length,
                         uint8_t* hash, uint32_t hash_length) {
    if (!crypto_ctx.initialized || !data || !hash || hash_length != 32) {
        return false;
    }

    if (crypto_ctx.config.use_hardware_acceleration) {
        return HW_Crypto_SHA256(data, length, hash);
    } else {
        // Simple software hash implementation
        uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        
        for (uint32_t i = 0; i < length; i++) {
            h[i % 8] = (h[i % 8] * 31 + data[i]) ^ crypto_ctx.config.key[i % 16];
        }
        
        memcpy(hash, h, 32);
        return true;
    }
}

bool Crypto_SecureStore(uint32_t id, const uint8_t* data, uint32_t length) {
    if (!crypto_ctx.initialized || !data || !crypto_ctx.config.enable_secure_storage) {
        return false;
    }

    enter_critical();

    SecureEntry* entry;
    bool entry_exists = find_secure_entry(id, &entry);

    // Check if we have enough space
    uint32_t required_space = entry_exists ? 0 : length;
    if (required_space > SECURE_STORAGE_SIZE) {
        exit_critical();
        return false;
    }

    if (!entry_exists) {
        if (crypto_ctx.entry_count >= MAX_SECURE_ENTRIES) {
            exit_critical();
            return false;
        }
        entry = &crypto_ctx.entries[crypto_ctx.entry_count++];
        entry->id = id;
        entry->offset = 0; // TODO: Implement proper storage allocation
    }

    entry->length = length;
    
    // Calculate hash of the data
    Crypto_CalculateHash(data, length, entry->hash, sizeof(entry->hash));

    // Encrypt and store the data
    uint32_t encrypted_length;
    if (!Crypto_Encrypt(data, length, 
                       &crypto_ctx.secure_storage[entry->offset],
                       &encrypted_length)) {
        exit_critical();
        return false;
    }

    Logger_Log(LOG_LEVEL_INFO, "CRYPTO", "Securely stored %d bytes for ID %d", 
               length, id);

    exit_critical();
    return true;
}

bool Crypto_SecureRetrieve(uint32_t id, uint8_t* data, uint32_t* length) {
    if (!crypto_ctx.initialized || !data || !length || 
        !crypto_ctx.config.enable_secure_storage) {
        return false;
    }

    enter_critical();

    SecureEntry* entry;
    if (!find_secure_entry(id, &entry)) {
        exit_critical();
        return false;
    }

    // Decrypt the data
    uint32_t decrypted_length;
    if (!Crypto_Decrypt(&crypto_ctx.secure_storage[entry->offset],
                       entry->length, data, &decrypted_length)) {
        exit_critical();
        return false;
    }

    // Verify hash
    uint8_t calculated_hash[32];
    Crypto_CalculateHash(data, decrypted_length, calculated_hash, sizeof(calculated_hash));
    
    if (memcmp(calculated_hash, entry->hash, sizeof(calculated_hash)) != 0) {
        Logger_Log(LOG_LEVEL_ERROR, "CRYPTO", "Hash verification failed for ID %d", id);
        exit_critical();
        return false;
    }

    *length = decrypted_length;
    
    Logger_Log(LOG_LEVEL_INFO, "CRYPTO", "Securely retrieved %d bytes for ID %d", 
               *length, id);

    exit_critical();
    return true;
}

