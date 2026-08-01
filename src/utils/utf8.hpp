#pragma once

#include <cstddef>
#include <string_view>

namespace ragcli::utils::utf8 {

// text[pos] 에서 시작하는 UTF-8 시퀀스를 디코딩해 코드포인트를 반환하고, 소비한
// 바이트 수를 out_len 에 채운다. pos 는 반드시 시퀀스 시작(비-continuation)
// 바이트를 가리켜야 한다. 잘린/손상된 시퀀스는 1바이트만 소비하고 해당 바이트
// 값을 그대로 반환한다 (호출자가 무한 루프 없이 항상 전진할 수 있도록 보장).
inline auto decode_codepoint(std::string_view text, std::size_t pos, std::size_t &out_len)
    -> char32_t {
    const auto b0 = static_cast<unsigned char>(text[pos]);
    if ((b0 & 0x80) == 0x00) {
        out_len = 1;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && pos + 1 < text.size()) {
        out_len = 2;
        return (static_cast<char32_t>(b0 & 0x1F) << 6) |
               static_cast<char32_t>(static_cast<unsigned char>(text[pos + 1]) & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0 && pos + 2 < text.size()) {
        out_len = 3;
        return (static_cast<char32_t>(b0 & 0x0F) << 12) |
               (static_cast<char32_t>(static_cast<unsigned char>(text[pos + 1]) & 0x3F) << 6) |
               static_cast<char32_t>(static_cast<unsigned char>(text[pos + 2]) & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0 && pos + 3 < text.size()) {
        out_len = 4;
        return (static_cast<char32_t>(b0 & 0x07) << 18) |
               (static_cast<char32_t>(static_cast<unsigned char>(text[pos + 1]) & 0x3F) << 12) |
               (static_cast<char32_t>(static_cast<unsigned char>(text[pos + 2]) & 0x3F) << 6) |
               static_cast<char32_t>(static_cast<unsigned char>(text[pos + 3]) & 0x3F);
    }
    out_len = 1;
    return b0;
}

// UTF-8 continuation 바이트(0b10xxxxxx)인지 판별한다.
inline auto is_continuation(unsigned char byte) -> bool {
    return (byte & 0xC0) == 0x80;
}

// [begin, byte_pos) 구간에서 continuation 바이트가 아닌 시작 바이트 수(코드포인트 수)를 센다.
inline auto char_count(std::string_view text, std::size_t byte_pos) -> std::size_t {
    std::size_t count = 0;
    for (std::size_t i = 0; i < byte_pos && i < text.size(); ++i) {
        if (!is_continuation(static_cast<unsigned char>(text[i]))) {
            ++count;
        }
    }
    return count;
}

// byte_pos 가 멀티바이트 시퀀스 중간이면 그 시퀀스의 시작 바이트까지 뒤로 이동시킨다.
inline auto snap_back(std::string_view text, std::size_t byte_pos) -> std::size_t {
    if (byte_pos >= text.size()) {
        return text.size();
    }
    while (byte_pos > 0 && is_continuation(static_cast<unsigned char>(text[byte_pos]))) {
        --byte_pos;
    }
    return byte_pos;
}

// byte_pos 에서 시작해 n 개의 코드포인트만큼 앞으로 이동한 바이트 오프셋을 반환한다.
// text 끝에 도달하면 text.size() 를 반환한다.
inline auto advance_chars(std::string_view text, std::size_t byte_pos, std::size_t n)
    -> std::size_t {
    std::size_t pos = snap_back(text, byte_pos);
    std::size_t remaining = n;
    while (remaining > 0 && pos < text.size()) {
        ++pos;
        while (pos < text.size() && is_continuation(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        --remaining;
    }
    return pos;
}

// byte_pos 에서 시작해 n 개의 코드포인트만큼 뒤로 이동한 바이트 오프셋을 반환한다.
// text 시작에 도달하면 0 을 반환한다.
inline auto retreat_chars(std::string_view text, std::size_t byte_pos, std::size_t n)
    -> std::size_t {
    std::size_t pos = snap_back(text, byte_pos);
    std::size_t remaining = n;
    while (remaining > 0 && pos > 0) {
        --pos;
        while (pos > 0 && is_continuation(static_cast<unsigned char>(text[pos]))) {
            --pos;
        }
        --remaining;
    }
    return pos;
}

} // namespace ragcli::utils::utf8
