#pragma once

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "chunking/chunk.hpp"
#include "chunking/chunker_interface.hpp"
#include "utils/base64.hpp"

namespace ragcli::chunking {

// Markdown 헤딩 (#, ##, ### 등)을 기준으로 문서를 섹션별로 분할하는 청커.
class MarkdownChunker : public Chunker {
  public:
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

            std::istringstream stream(page.text);
            std::string line;
            std::string current_heading = page.title;
            std::string current_section;

            auto flush_section = [&]() {
                if (current_section.empty()) {
                    return;
                }
                Chunk chunk;
                chunk.text = current_section;
                chunk.title = current_heading;
                chunk.source = source_name;
                chunk.page_index = page.page_index;
                chunk.chunk_index = chunk_index++;
                chunks.push_back(std::move(chunk));
                current_section.clear();
            };

            while (std::getline(stream, line)) {
                if (is_heading_line(line)) {
                    flush_section();
                    current_heading = extract_heading_title(line);
                    current_section += line + "\n";
                } else {
                    current_section += line + "\n";
                }
            }
            flush_section();
        }

        return chunks;
    }

  private:
    static auto is_heading_line(const std::string &line) -> bool {
        if (line.empty() || line[0] != '#') {
            return false;
        }
        std::size_t i = 0;
        while (i < line.size() && line[i] == '#') {
            ++i;
        }
        return i > 0 && i < line.size() && line[i] == ' ';
    }

    static auto extract_heading_title(const std::string &line) -> std::string {
        std::size_t pos = line.find_first_not_of('#');
        if (pos == std::string::npos) {
            return line;
        }
        if (pos < line.size() && line[pos] == ' ') {
            ++pos;
        }
        return line.substr(pos);
    }
};

} // namespace ragcli::chunking
