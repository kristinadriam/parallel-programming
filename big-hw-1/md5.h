#ifndef MD5_H
#define MD5_H

#include <cstdint>
#include <cstring>
#include <string>

class MD5 {
public:
    MD5();
    void update(const uint8_t* data, size_t len);
    void update(const std::string& s);
    void finalize();
    std::string hex_digest() const;
    void raw_digest(uint8_t out[16]) const;

    static std::string hash(const std::string& input);
    static void hash_raw(const std::string& input, uint8_t out[16]);

private:
    uint32_t state_[4];
    uint64_t count_;
    uint8_t buffer_[64];
    uint8_t digest_[16];
    bool finalized_;

    void transform(const uint8_t block[64]);
};

#endif
