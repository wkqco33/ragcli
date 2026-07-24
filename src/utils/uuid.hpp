#pragma once

#include <array>
#include <random>
#include <string>

namespace ragcli::utils {

// UUID v4 형식의 무작위 문자열을 생성한다.
// 충분한 엔트로피가 필요하지 않은 식별자 용도로 사용한다.
inline auto generate_uuid_v4() -> std::string {
    static std::random_device rd_device;
    static std::mt19937 gen(rd_device());
    static constexpr int k_hex_max_index = 15;
    static constexpr int k_variant_low = 8;
    static constexpr int k_variant_high = 11;
    static std::uniform_int_distribution<uint32_t> dis(0, k_hex_max_index);
    static std::uniform_int_distribution<uint32_t> dis2(k_variant_low, k_variant_high);

    static constexpr std::array<char, 17> hex = {'0', '1', '2', '3', '4', '5', '6', '7', '8',
                                                 '9', 'a', 'b', 'c', 'd', 'e', 'f', '\0'};

    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (char &ch_ref : uuid) {
        if (ch_ref == 'x') {
            ch_ref = hex[dis(gen)];
        } else if (ch_ref == 'y') {
            ch_ref = hex[dis2(gen)];
        }
    }
    return uuid;
}

} // namespace ragcli::utils
