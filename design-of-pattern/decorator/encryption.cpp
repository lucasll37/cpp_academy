#include "encryption.hpp"
#include <fmt/core.h>

namespace patterns {

static std::string xor_cipher(const std::string& data) {
    std::string result = data;
    for (auto& c : result) c ^= 0x5A;
    return result;
}

void Encryption::write(const std::string& data) {
    std::string encrypted = xor_cipher(data);
    fmt::println("  Encryption::write   → (encrypted {} bytes)", encrypted.size());
    wrapped_->write(encrypted);
}

std::string Encryption::read() {
    std::string raw = wrapped_->read();
    std::string decrypted = xor_cipher(raw); // xor é simétrico
    fmt::println("  Encryption::read    ← (decrypted {} bytes)", decrypted.size());
    return decrypted;
}

} // namespace patterns