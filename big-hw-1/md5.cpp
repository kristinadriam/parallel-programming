#include "md5.h"

static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
static inline uint32_t rotate_left(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static const uint32_t T[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const int S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static void encode(uint8_t* out, const uint32_t* in, size_t len) {
    for (size_t i = 0, j = 0; j < len; i++, j += 4) {
        out[j]     = static_cast<uint8_t>(in[i] & 0xff);
        out[j + 1] = static_cast<uint8_t>((in[i] >> 8) & 0xff);
        out[j + 2] = static_cast<uint8_t>((in[i] >> 16) & 0xff);
        out[j + 3] = static_cast<uint8_t>((in[i] >> 24) & 0xff);
    }
}

static void decode(uint32_t* out, const uint8_t* in, size_t len) {
    for (size_t i = 0, j = 0; j < len; i++, j += 4) {
        out[i] = static_cast<uint32_t>(in[j])
               | (static_cast<uint32_t>(in[j + 1]) << 8)
               | (static_cast<uint32_t>(in[j + 2]) << 16)
               | (static_cast<uint32_t>(in[j + 3]) << 24);
    }
}

MD5::MD5() : count_(0), finalized_(false) {
    state_[0] = 0x67452301;
    state_[1] = 0xefcdab89;
    state_[2] = 0x98badcfe;
    state_[3] = 0x10325476;
    std::memset(buffer_, 0, sizeof(buffer_));
    std::memset(digest_, 0, sizeof(digest_));
}

void MD5::transform(const uint8_t block[64]) {
    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    uint32_t x[16];
    decode(x, block, 64);

    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16) {
            f = F(b, c, d);
            g = i;
        } else if (i < 32) {
            f = G(b, c, d);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = H(b, c, d);
            g = (3 * i + 5) % 16;
        } else {
            f = I(b, c, d);
            g = (7 * i) % 16;
        }
        uint32_t temp = d;
        d = c;
        c = b;
        b = b + rotate_left(a + f + T[i] + x[g], S[i]);
        a = temp;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
}

void MD5::update(const uint8_t* data, size_t len) {
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

void MD5::update(const std::string& s) {
    update(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

void MD5::finalize() {
    if (finalized_) return;

    uint8_t padding[64] = {0x80};
    uint64_t bits = count_ * 8;
    size_t index = count_ % 64;
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);

    update(padding, pad_len);

    uint8_t bits_buf[8];
    bits_buf[0] = static_cast<uint8_t>(bits);
    bits_buf[1] = static_cast<uint8_t>(bits >> 8);
    bits_buf[2] = static_cast<uint8_t>(bits >> 16);
    bits_buf[3] = static_cast<uint8_t>(bits >> 24);
    bits_buf[4] = static_cast<uint8_t>(bits >> 32);
    bits_buf[5] = static_cast<uint8_t>(bits >> 40);
    bits_buf[6] = static_cast<uint8_t>(bits >> 48);
    bits_buf[7] = static_cast<uint8_t>(bits >> 56);
    update(bits_buf, 8);

    encode(digest_, state_, 16);
    finalized_ = true;
}

std::string MD5::hex_digest() const {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(32);
    for (int i = 0; i < 16; i++) {
        result += hex_chars[digest_[i] >> 4];
        result += hex_chars[digest_[i] & 0x0f];
    }
    return result;
}

void MD5::raw_digest(uint8_t out[16]) const {
    std::memcpy(out, digest_, 16);
}

std::string MD5::hash(const std::string& input) {
    MD5 ctx;
    ctx.update(input);
    ctx.finalize();
    return ctx.hex_digest();
}

void MD5::hash_raw(const std::string& input, uint8_t out[16]) {
    MD5 ctx;
    ctx.update(input);
    ctx.finalize();
    ctx.raw_digest(out);
}
