#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "document/directory_source.hpp"

namespace {

namespace fs = std::filesystem;

// 1x1 투명 GIF89a. stb_image 가 실제로 디코딩할 수 있는 최소 유효 GIF 페이로드다.
constexpr std::array<unsigned char, 34> k_tiny_gif = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x01, 0x4c, 0x00, 0x3b};

class DirectorySourceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "ragcli_directory_source_test";
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }

    void TearDown() override {
        fs::remove_all(dir_);
    }

    fs::path dir_;
};

TEST_F(DirectorySourceTest, IndexesGifFiles) {
    std::ofstream gif(dir_ / "pixel.gif", std::ios::binary);
    gif.write(reinterpret_cast<const char *>(k_tiny_gif.data()),
              static_cast<std::streamsize>(k_tiny_gif.size()));
    gif.close();

    ragcli::document::DirectorySource source(dir_.string());
    auto pages = source.extract();

    ASSERT_EQ(pages.size(), 1U);
    EXPECT_TRUE(pages[0].is_image);
    EXPECT_EQ(pages[0].image_width, 1);
    EXPECT_EQ(pages[0].image_height, 1);
}

TEST_F(DirectorySourceTest, SkipsUndecodableWebpButKeepsOtherFiles) {
    // stb_image 는 webp 디코더가 없으므로 항상 디코딩에 실패한다. DirectorySource
    // 의 결함 허용 로직이 이를 경고 로그와 함께 건너뛰고 나머지 파일은 계속
    // 처리해야 한다 (디렉터리 전체 인덱싱이 중단되면 안 된다).
    std::ofstream webp(dir_ / "photo.webp", std::ios::binary);
    webp << "RIFF....WEBPVP8 not-a-real-payload";
    webp.close();

    std::ofstream text(dir_ / "note.txt");
    text << "hello from directory source test";
    text.close();

    ragcli::document::DirectorySource source(dir_.string());
    auto pages = source.extract();

    ASSERT_EQ(pages.size(), 1U);
    EXPECT_FALSE(pages[0].is_image);
    EXPECT_NE(pages[0].text.find("hello from directory source test"), std::string::npos);
}

} // namespace
