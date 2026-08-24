#include "utils/image_utils.hpp"
#include <gtest/gtest.h>

namespace ragcli::utils {

TEST(ImageUtils, SmallImageIsNotDownsampled) {
    std::vector<uint8_t> dummy_rgba(100 * 100 * 4, 128);
    auto [scaled, new_w, new_h] = downsample_rgba_if_needed(dummy_rgba, 100, 100);
    EXPECT_EQ(new_w, 100);
    EXPECT_EQ(new_h, 100);
    EXPECT_EQ(scaled.size(), dummy_rgba.size());
}

TEST(ImageUtils, LargeImageIsDownsampledBelowMaxBytes) {
    // 3000 x 2000 픽셀 = 6,000,000 픽셀 = 24 MB raw RGBA
    int w = 3000;
    int h = 2000;
    std::vector<uint8_t> large_rgba(static_cast<std::size_t>(w) * h * 4, 200);

    constexpr std::size_t max_bytes = 4 * 1024 * 1024; // 4 MB 제한
    auto [scaled, new_w, new_h] = downsample_rgba_if_needed(large_rgba, w, h, max_bytes);

    EXPECT_LT(scaled.size(), large_rgba.size());
    EXPECT_LE(scaled.size(), max_bytes);
    EXPECT_LT(new_w, w);
    EXPECT_LT(new_h, h);
}

} // namespace ragcli::utils
