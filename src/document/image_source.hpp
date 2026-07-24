#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "stb_image.h"

#include "document/document_source.hpp"

namespace ragcli::document {

// 이미지 파일을 디코딩해 RGBA 픽셀 데이터를 ExtractedPage 에 담는다.
class ImageFileSource : public DocumentSource {
  public:
    explicit ImageFileSource(std::string path) : path_(std::move(path)) {}

    auto extract() const -> std::vector<ExtractedPage> override {
        int width = 0;
        int height = 0;
        int channels = 0;

        unsigned char *pixels =
            stbi_load(path_.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (pixels == nullptr) {
            const char *reason = stbi_failure_reason();
            throw std::runtime_error("Failed to decode image: " + path_ + " (" +
                                     std::string(reason ? reason : "unknown reason") + ")");
        }

        if (width <= 0 || height <= 0) {
            stbi_image_free(pixels);
            throw std::runtime_error("Invalid image dimensions for: " + path_);
        }

        const std::size_t pixel_count =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;

        ExtractedPage page;
        page.title = std::filesystem::path(path_).filename().string();
        page.text = "[Image] " + path_;
        page.image.assign(pixels, pixels + pixel_count);
        page.image_width = width;
        page.image_height = height;
        page.is_image = true;

        stbi_image_free(pixels);
        return {std::move(page)};
    }

    auto source_name() const -> std::string override {
        return path_;
    }

  private:
    std::string path_;
};

} // namespace ragcli::document
