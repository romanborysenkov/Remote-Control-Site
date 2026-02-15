#include "sha1.h"
#include <cstring>
#include <cstdint>

namespace sha1 {

namespace {
    // SHA1 константи
    const uint32_t h0 = 0x67452301;
    const uint32_t h1 = 0xEFCDAB89;
    const uint32_t h2 = 0x98BADCFE;
    const uint32_t h3 = 0x10325476;
    const uint32_t h4 = 0xC3D2E1F0;

    inline uint32_t leftRotate(uint32_t value, uint32_t amount) {
        return (value << amount) | (value >> (32 - amount));
    }

    void processChunk(const uint8_t* chunk, uint32_t* h) {
        uint32_t w[80];
        
        // Копіюємо chunk у перші 16 слів
        for (int i = 0; i < 16; i++) {
            w[i] = (chunk[i * 4] << 24) | (chunk[i * 4 + 1] << 16) |
                   (chunk[i * 4 + 2] << 8) | chunk[i * 4 + 3];
        }
        
        // Розширюємо до 80 слів
        for (int i = 16; i < 80; i++) {
            w[i] = leftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        
        uint32_t a = h[0];
        uint32_t b = h[1];
        uint32_t c = h[2];
        uint32_t d = h[3];
        uint32_t e = h[4];
        
        // Основний цикл SHA1
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            
            uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = leftRotate(b, 30);
            b = a;
            a = temp;
        }
        
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }
}

std::string hash(const unsigned char* data, size_t len) {
    uint32_t h[5] = {h0, h1, h2, h3, h4};
    
    // Обчислюємо кількість повних блоків
    size_t originalLen = len;
    size_t totalLen = len + 9; // +1 байт для 0x80, +8 байт для довжини
    size_t paddedLen = ((totalLen + 63) / 64) * 64;
    
    // Створюємо буфер з padding
    uint8_t* padded = new uint8_t[paddedLen];
    std::memset(padded, 0, paddedLen);
    std::memcpy(padded, data, len);
    padded[len] = 0x80;
    
    // Додаємо довжину в бітах як 64-бітне число (big-endian)
    uint64_t bitLen = originalLen * 8;
    padded[paddedLen - 8] = (bitLen >> 56) & 0xFF;
    padded[paddedLen - 7] = (bitLen >> 48) & 0xFF;
    padded[paddedLen - 6] = (bitLen >> 40) & 0xFF;
    padded[paddedLen - 5] = (bitLen >> 32) & 0xFF;
    padded[paddedLen - 4] = (bitLen >> 24) & 0xFF;
    padded[paddedLen - 3] = (bitLen >> 16) & 0xFF;
    padded[paddedLen - 2] = (bitLen >> 8) & 0xFF;
    padded[paddedLen - 1] = bitLen & 0xFF;
    
    // Обробляємо кожен 512-бітний блок
    for (size_t i = 0; i < paddedLen; i += 64) {
        processChunk(padded + i, h);
    }
    
    delete[] padded;
    
    // Конвертуємо результат у бінарний рядок (20 байт)
    std::string result;
    result.reserve(20);
    for (int i = 0; i < 5; i++) {
        result += static_cast<char>((h[i] >> 24) & 0xFF);
        result += static_cast<char>((h[i] >> 16) & 0xFF);
        result += static_cast<char>((h[i] >> 8) & 0xFF);
        result += static_cast<char>(h[i] & 0xFF);
    }
    
    return result;
}

} // namespace sha1
