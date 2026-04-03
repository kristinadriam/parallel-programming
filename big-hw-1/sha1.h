#ifndef SHA1_H
#define SHA1_H

#include <cstdint>
#include <cstring>
#include <string>

class SHA1 {
public:
    SHA1();
    void update(const uint8_t* data, size_t len);
    void update(const std::string& s);
    void finalize();
    std::string hex_digest() const;

    static std::string hash(const std::string& input);
    static std::string hash_raw(const uint8_t* data, size_t len);

private:
    uint32_t state_[5];
    uint64_t count_;
    uint8_t buffer_[64];
    uint8_t digest_[20];
    bool finalized_;

    void transform(const uint8_t block[64]);
};

#endif
