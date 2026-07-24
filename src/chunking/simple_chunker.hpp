#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "chunking/chunk.hpp"
#include "chunking/chunker_interface.hpp"
#include "utils/base64.hpp"

namespace ragcli::chunking {

// 고정 길이 + 오버랩 기반 텍스트 청커.
class SimpleChunker : public Chunker {
  public:
    explicit SimpleChunker(std::size_t chunk_size = 512, std::size_t overlap = 64)
        : chunk_size_(chunk_size), overlap_(overlap) {
        if (overlap_ >= chunk_size_) {
            overlap_ = chunk_size_ / 2;
        }
    }

    auto chunk(const std::vector<document::ExtractedPage> &pages,
               const std::string &source_name) const -> std::vector<Chunk> override {
        std::vector<Chunk> chunks;
        int chunk_index = 0;

        for (const auto &page : pages) {
            if (page.is_image) {
                Chunk chunk;
                chunk.text = page.text;
                chunk.title = page.title;
                chunk.source = source_name;
                chunk.page_index = page.page_index;
                chunk.chunk_index = chunk_index++;
                chunk.is_image = true;
                chunk.image_width = page.image_width;
                chunk.image_height = page.image_height;
                chunk.image = page.image;
                if (!page.image.empty()) {
                    chunk.image_base64 = utils::base64_encode(page.image);
                }
                chunks.push_back(std::move(chunk));
                continue;
            }

            if (page.text.empty()) {
                continue;
            }

            const std::size_t total = page.text.size();
            std::size_t start = 0;

            while (start < total) {
                std::size_t end = std::min(start + chunk_size_, total);

                // 단어 경계에서 자르도록 조정.
                if (end < total) {
                    end = find_word_boundary(page.text, end);
                }

                Chunk chunk;
                chunk.text = page.text.substr(start, end - start);
                chunk.title = page.title;
                chunk.source = source_name;
                chunk.page_index = page.page_index;
                chunk.chunk_index = chunk_index++;
                chunks.push_back(std::move(chunk));

                if (end >= total) {
                    break;
                }
                start = end - overlap_;
            }
        }

        return chunks;
    }

  private:
    static auto find_word_boundary(const std::string &text, std::size_t pos) -> std::size_t {
        const std::size_t max_lookback = 20;
        std::size_t start = pos > max_lookback ? pos - max_lookback : 0;
        for (std::size_t i = pos; i > start; --i) {
            if (text[i - 1] == ' ' || text[i - 1] == '\n' || text[i - 1] == '\t') {
                return i;
            }
        }
        return pos;
    }

    std::size_t chunk_size_;
    std::size_t overlap_;
};

} // namespace ragcli::chunking
