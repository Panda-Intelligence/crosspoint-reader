#pragma once

#include <Arduino.h>

namespace CryptoUtils {

inline void generateRandomToken(char* buffer, size_t maxLen) {
    if (maxLen < 17) return; // need 16 chars + null
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (size_t i = 0; i < 16; i++) {
        buffer[i] = charset[random(0, sizeof(charset) - 1)];
    }
    buffer[16] = '\0';
}

} // namespace CryptoUtils
