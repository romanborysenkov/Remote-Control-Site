#pragma once

#include <cstddef>
#include <string>

namespace sha1 {
// Повертає 20 байт (binary) SHA1 хеш у рядку.
std::string hash(const unsigned char* data, size_t len);
inline std::string hash(const std::string& s) {
    return hash(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}
}
