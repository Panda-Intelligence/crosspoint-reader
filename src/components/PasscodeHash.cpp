#include "PasscodeHash.h"

#include <esp_random.h>
#include <mbedtls/sha256.h>

#include <cstring>

namespace passcode {

namespace {

constexpr char kHexChars[] = "0123456789abcdef";

void bytesToHex(const uint8_t* bytes, size_t len, char* out) {
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = kHexChars[(bytes[i] >> 4) & 0x0F];
    out[i * 2 + 1] = kHexChars[bytes[i] & 0x0F];
  }
  out[len * 2] = '\0';
}

uint8_t hexNibble(char c) {
  if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
  if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
  return 0;
}

void hexToBytes(const char* hex, size_t hexLen, uint8_t* out) {
  for (size_t i = 0; i < hexLen / 2; i++) {
    out[i] = static_cast<uint8_t>((hexNibble(hex[i * 2]) << 4) | hexNibble(hex[i * 2 + 1]));
  }
}

}  // namespace

void generateSaltHex(char* out) {
  uint8_t salt[kSaltBytes];
  // esp_random() returns 32 bits of hardware randomness; gather 16 bytes.
  for (size_t i = 0; i < kSaltBytes; i += 4) {
    const uint32_t r = esp_random();
    salt[i] = static_cast<uint8_t>(r & 0xFF);
    salt[i + 1] = static_cast<uint8_t>((r >> 8) & 0xFF);
    salt[i + 2] = static_cast<uint8_t>((r >> 16) & 0xFF);
    salt[i + 3] = static_cast<uint8_t>((r >> 24) & 0xFF);
  }
  bytesToHex(salt, kSaltBytes, out);
}

void hashPasscode(const char* saltHex, const char* code, char* out) {
  // Stack-allocated combined buffer: 16 bytes salt + 4 bytes code = 20 bytes.
  uint8_t combined[kSaltBytes + kPasscodeLen];
  hexToBytes(saltHex, kSaltHexLen, combined);
  // Append code ASCII bytes; if shorter than expected, treat missing chars as 0.
  for (size_t i = 0; i < kPasscodeLen; i++) {
    combined[kSaltBytes + i] = static_cast<uint8_t>(code[i] ? code[i] : '0');
  }

  uint8_t digest[kHashBytes];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, /*is224=*/0);
  mbedtls_sha256_update(&ctx, combined, sizeof(combined));
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);

  // Wipe the combined buffer to reduce passcode residue on the stack.
  // (Best-effort: compiler may elide; this is not a hardened-crypto context.)
  volatile uint8_t* p = combined;
  for (size_t i = 0; i < sizeof(combined); i++) p[i] = 0;

  bytesToHex(digest, kHashBytes, out);
}

bool constantTimeEquals(const char* a, const char* b) {
  // Compare lengths first; bail early if mismatched (length itself is not secret).
  const size_t la = strlen(a);
  const size_t lb = strlen(b);
  if (la != lb) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < la; i++) {
    diff |= static_cast<uint8_t>(a[i] ^ b[i]);
  }
  return diff == 0;
}

}  // namespace passcode
