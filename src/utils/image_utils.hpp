#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>

namespace ragcli::utils {

// RGBA 픽셀 데이터의 가로/세로/채널 정보와 원본 바이트 수 기반다운샘플링.
// 기본 max_raw_bytes = 8MB (8,388,608 bytes) -> Base64 인코딩 시 약 11.1MB 문자열 생성.
inline auto downsample_rgba_if_needed(const std::vector<uint8_t> &image_data,
                                      int width, int height,
                                      std::size_t max_raw_bytes = 8 * 1024 * 1024)
    -> std::tuple<std::vector<uint8_t>, int, int> {
    if (image_data.empty() || width <= 0 || height <= 0 || image_data.size() <= max_raw_bytes) {
        return {image_data, width, height};
    }

    // 다운샘플링 축소 비율 (scale factor) 계산
    int scale = 1;
    while (static_cast<std::size_t>((width / scale) * (height / scale) * 4) > max_raw_bytes &&
           scale < 16) {
        scale++;
    }

    if (scale <= 1) {
        return {image_data, width, height};
    }

    const int new_w = width / scale;
    const int new_h = height / scale;
    if (new_w <= 0 || new_h <= 0) {
        return {image_data, width, height};
    }

    std::vector<uint8_t> scaled;
    scaled.resize(static_cast<std::size_t>(new_w) * static_cast<std::size_t>(new_h) * 4U);

    for (int ny = 0; ny < new_h; ++ny) {
        for (int nx = 0; nx < new_w; ++nx) {
            uint32_t r_sum = 0, g_sum = 0, b_sum = 0, a_sum = 0;
            int count = 0;
            for (int sy = 0; sy < scale; ++sy) {
                int oy = ny * scale + sy;
                if (oy >= height) break;
                for (int sx = 0; sx < scale; ++sx) {
                    int ox = nx * scale + sx;
                    if (ox >= width) break;
                    std::size_t orig_idx = (static_cast<std::size_t>(oy) * width + ox) * 4U;
                    if (orig_idx + 3 < image_data.size()) {
                        r_sum += image_data[orig_idx + 0];
                        g_sum += image_data[orig_idx + 1];
                        b_sum += image_data[orig_idx + 2];
                        a_sum += image_data[orig_idx + 3];
                        count++;
                    }
                }
            }
            std::size_t new_idx = (static_cast<std::size_t>(ny) * new_w + nx) * 4U;
            if (count > 0) {
                scaled[new_idx + 0] = static_cast<uint8_t>(r_sum / count);
                scaled[new_idx + 1] = static_cast<uint8_t>(g_sum / count);
                scaled[new_idx + 2] = static_cast<uint8_t>(b_sum / count);
                scaled[new_idx + 3] = static_cast<uint8_t>(a_sum / count);
            }
        }
    }

    return {scaled, new_w, new_h};
}

} // namespace ragcli::utils
