#pragma once

#include <string>
#include <utility>
#include <vector>

#include "chunking/chunk.hpp"
#include "chunking/chunker_interface.hpp"
#include "utils/base64.hpp"
#include "utils/image_utils.hpp"

namespace ragcli::chunking {

// 페이지 하나를 하나의 Chunk 로 변환한다. (짧은 문서나 이미지용)
class NoChunker : public Chunker {
  public:
    auto chunk(const std::vector<document::ExtractedPage> &pages,
               const std::string &source_name) const -> std::vector<Chunk> override {
        std::vector<Chunk> chunks;
        chunks.reserve(pages.size());

        int chunk_index = 0;
        for (const auto &page : pages) {
            Chunk chunk;
            chunk.text = page.text;
            chunk.title = page.title;
            chunk.source = page.source_path.empty() ? source_name : page.source_path;
            chunk.page_index = page.page_index;
            chunk.chunk_index = chunk_index++;
            chunk.is_image = page.is_image;
            chunk.image_width = page.image_width;
            chunk.image_height = page.image_height;
            chunk.image = page.image;
            if (!page.image.empty()) {
                auto [scaled_image, new_w, new_h] = utils::downsample_rgba_if_needed(
                    page.image, page.image_width, page.image_height);
                chunk.image_width = new_w;
                chunk.image_height = new_h;
                chunk.image = scaled_image;
                chunk.image_base64 = utils::base64_encode(scaled_image);
            }
            chunks.push_back(std::move(chunk));
        }

        return chunks;
    }
};

} // namespace ragcli::chunking
