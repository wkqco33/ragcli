#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ragcli::utils {

// 바이너리 데이터(예: 이미지 픽셀/바이트)를 Base64 문자열로 인코딩한다.
inline auto base64_encode(const std::vector<uint8_t> &data) -> std::string {
    static constexpr std::array<char, 65> base64_chars = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0'};
    static constexpr int base64_bits = 6;
    static constexpr int byte_bits = 8;
    static constexpr uint32_t base64_mask = 0x3F;

    std::string ret;
    ret.reserve(((data.size() + 2) / 3) * 4);

    uint32_t val = 0;
    int valb = -base64_bits;
    for (uint8_t byte : data) {
        val = (val << byte_bits) + byte;
        valb += byte_bits;
        while (valb >= 0) {
            ret.push_back(base64_chars[(val >> valb) & base64_mask]);
            valb -= base64_bits;
        }
    }
    if (valb > -base64_bits) {
        ret.push_back(base64_chars[((val << byte_bits) >> (valb + byte_bits)) & base64_mask]);
    }
    while (ret.size() % 4 != 0) {
        ret.push_back('=');
    }
    return ret;
}

} // namespace ragcli::utils
