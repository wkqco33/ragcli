#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ragcli::utils {

// 바이너리 데이터(예: 이미지 픽셀/바이트)를 Base64 문자열로 인코딩한다.
inline auto base64_encode(const std::vector<uint8_t> &data) -> std::string {
    static constexpr char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string ret;
    ret.reserve(((data.size() + 2) / 3) * 4);

    uint32_t val = 0;
    int valb = -6;
    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            ret.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        ret.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (ret.size() % 4 != 0) {
        ret.push_back('=');
    }
    return ret;
}

} // namespace ragcli::utils
