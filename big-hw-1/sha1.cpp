#include "sha1.h"

static inline uint32_t rotate_left(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

SHA1::SHA1() : count_(0), finalized_(false) {
    state_[0] = 0x67452301;
    state_[1] = 0xEFCDAB89;
    state_[2] = 0x98BADCFE;
    state_[3] = 0x10325476;
    state_[4] = 0xC3D2E1F0;
    std::memset(buffer_, 0, sizeof(buffer_));
    std::memset(digest_, 0, sizeof(digest_));
}

void SHA1::transform(const uint8_t block[64]) {
    uint32_t w[80];

    for (int i = 0; i < 16; i++) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24)
             | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
             | (static_cast<uint32_t>(block[i * 4 + 2]) << 8)
             | (static_cast<uint32_t>(block[i * 4 + 3]));
    }

    for (int i = 16; i < 80; i++) {
        w[i] = rotate_left(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
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

        uint32_t temp = rotate_left(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotate_left(b, 30);
        b = a;
        a = temp;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
}

void SHA1::update(const uint8_t* data, size_t len) {
    size_t index = count_ % 64;
    count_ += len;

    size_t i = 0;
    if (index) {
        size_t part_len = 64 - index;
        if (len >= part_len) {
            std::memcpy(buffer_ + index, data, part_len);
            transform(buffer_);
            i = part_len;
        } else {
            std::memcpy(buffer_ + index, data, len);
            return;
        }
    }

    for (; i + 64 <= len; i += 64) {
        transform(data + i);
    }

    if (i < len) {
        std::memcpy(buffer_, data + i, len - i);
    }
}

void SHA1::update(const std::string& s) {
    update(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

void SHA1::finalize() {
    if (finalized_) return;

    uint64_t bits = count_ * 8;

    uint8_t pad_byte = 0x80;
    update(&pad_byte, 1);

    uint8_t zero = 0;
    while ((count_ % 64) != 56) {
        update(&zero, 1);
    }

    uint8_t len_buf[8];
    len_buf[0] = static_cast<uint8_t>(bits >> 56);
    len_buf[1] = static_cast<uint8_t>(bits >> 48);
    len_buf[2] = static_cast<uint8_t>(bits >> 40);
    len_buf[3] = static_cast<uint8_t>(bits >> 32);
    len_buf[4] = static_cast<uint8_t>(bits >> 24);
    len_buf[5] = static_cast<uint8_t>(bits >> 16);
    len_buf[6] = static_cast<uint8_t>(bits >> 8);
    len_buf[7] = static_cast<uint8_t>(bits);
    update(len_buf, 8);

    for (int i = 0; i < 5; i++) {
        digest_[i * 4]     = static_cast<uint8_t>(state_[i] >> 24);
        digest_[i * 4 + 1] = static_cast<uint8_t>(state_[i] >> 16);
        digest_[i * 4 + 2] = static_cast<uint8_t>(state_[i] >> 8);
        digest_[i * 4 + 3] = static_cast<uint8_t>(state_[i]);
    }
    finalized_ = true;
}

std::string SHA1::hex_digest() const {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(40);
    for (int i = 0; i < 20; i++) {
        result += hex_chars[digest_[i] >> 4];
        result += hex_chars[digest_[i] & 0x0f];
    }
    return result;
}

std::string SHA1::hash(const std::string& input) {
    SHA1 ctx;
    ctx.update(input);
    ctx.finalize();
    return ctx.hex_digest();
}

std::string SHA1::hash_raw(const uint8_t* data, size_t len) {
    SHA1 ctx;
    ctx.update(data, len);
    ctx.finalize();
    return ctx.hex_digest();
}
