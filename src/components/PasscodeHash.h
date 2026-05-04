#pragma once

#include <cstddef>
#include <cstdint>

namespace passcode {

// Salt: 16 bytes raw → 32 hex chars + NUL.
constexpr size_t kSaltBytes = 16;
constexpr size_t kSaltHexLen = kSaltBytes * 2;
// SHA-256: 32 bytes raw → 64 hex chars + NUL.
constexpr size_t kHashBytes = 32;
constexpr size_t kHashHexLen = kHashBytes * 2;
// 4-digit passcode.
constexpr size_t kPasscodeLen = 4;

// Generate a fresh 16-byte salt via esp_random() and write it as 32 hex chars
// + NUL into out (must be at least kSaltHexLen+1 bytes).
void generateSaltHex(char* out);

// Compute SHA-256(salt_bytes || code_ascii) and write 64 hex chars + NUL into
// out (at least kHashHexLen+1 bytes). saltHex must be exactly kSaltHexLen
// characters of valid hex; non-hex bytes are treated as zero. code is the
// 4-character ASCII passcode.
void hashPasscode(const char* saltHex, const char* code, char* out);

// Constant-time comparison of two NUL-terminated strings of equal length.
// Returns true if equal. If lengths differ, returns false (and bails early).
bool constantTimeEquals(const char* a, const char* b);

}  // namespace passcode
