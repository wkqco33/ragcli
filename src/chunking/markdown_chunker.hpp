#pragma once

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "chunking/chunk.hpp"
#include "chunking/chunker_interface.hpp"
#include "chunking/simple_chunker.hpp"
#include "utils/base64.hpp"
#include "utils/image_utils.hpp"
#include "utils/utf8.hpp"

namespace ragcli::chunking {

// Markdown 헤딩 (#, ##, ### 등)을 기준으로 문서를 섹션별로 분할하는 청커.
// 헤딩 계층 경로(heading_path)를 보존하고, 펜스 코드 블록 안의 '#'은 헤딩으로
// 취급하지 않으며, 섹션이 chunk_size 를 넘으면 SimpleChunker 로 재분할한다.
class MarkdownChunker : public Chunker {
  public:
    explicit MarkdownChunker(ChunkSize chunk_size = ChunkSize{k_default_chunk_size},
                             Overlap overlap = Overlap{k_default_overlap})
        : chunk_size_(chunk_size.value), overlap_(overlap.value),
          fallback_chunker_(chunk_size, overlap) {}

    auto chunk(const std::vector<document::ExtractedPage> &pages,
               const std::string &source_name) const -> std::vector<Chunk> override {
        std::vector<Chunk> chunks;
        int chunk_index = 0;

        for (const auto &page : pages) {
            const std::string source = page.source_path.empty() ? source_name : page.source_path;

            if (page.is_image) {
                Chunk chunk;
                chunk.text = page.text;
                chunk.title = page.title;
                chunk.source = source;
                chunk.page_index = page.page_index;
                chunk.chunk_index = chunk_index++;
                chunk.is_image = true;
                chunk.image_width = page.image_width;
                chunk.image_height = page.image_height;
                if (!page.image.empty()) {
                    auto [scaled_image, new_w, new_h] = utils::downsample_rgba_if_needed(
                        page.image, page.image_width, page.image_height);
                    chunk.image_width = new_w;
                    chunk.image_height = new_h;
                    chunk.image = std::move(scaled_image);
                    chunk.image_base64 = utils::base64_encode(chunk.image);
                }
                chunks.push_back(std::move(chunk));
                continue;
            }

            if (page.text.empty()) {
                continue;
            }

            for (auto &section : split_sections(page.text, page.title)) {
                const std::size_t section_chars =
                    utils::utf8::char_count(section.text, section.text.size());

                if (section_chars > chunk_size_) {
                    document::ExtractedPage sub_page;
                    sub_page.text = section.text;
                    sub_page.title = section.heading;
                    sub_page.page_index = page.page_index;
                    sub_page.source_path = source;

                    auto sub_chunks = fallback_chunker_.chunk({sub_page}, source_name);
                    for (auto &sub_chunk : sub_chunks) {
                        sub_chunk.heading_path = section.heading_path;
                        sub_chunk.chunk_index = chunk_index++;
                        chunks.push_back(std::move(sub_chunk));
                    }
                    continue;
                }

                Chunk c;
                c.text = std::move(section.text);
                c.title = section.heading;
                c.heading_path = section.heading_path;
                c.source = source;
                c.page_index = page.page_index;
                c.chunk_index = chunk_index++;
                chunks.push_back(std::move(c));
            }
        }

        return chunks;
    }

  private:
    struct Section {
        std::string heading;
        std::string heading_path;
        std::string text;
    };

    // 페이지 텍스트를 헤딩 경계로 분할한다. chunk_size_/4 문자 미만인 섹션은
    // (헤딩만 있고 내용이 거의 없는 경우 등) 다음 섹션과 합쳐진다.
    auto split_sections(const std::string &text,
                        const std::string &page_title) const -> std::vector<Section> {
        std::vector<Section> sections;
        std::istringstream stream(text);
        std::string line;

        std::vector<std::string> heading_stack;
        std::string current_heading = page_title;
        std::string current_heading_path = page_title;
        std::string current_section;
        bool in_fence = false;

        const std::size_t min_chars = (std::max<std::size_t>)(chunk_size_ / 4, 1);

        auto flush = [&](bool force) {
            if (current_section.empty()) {
                return;
            }
            const std::size_t char_len =
                utils::utf8::char_count(current_section, current_section.size());
            if (!force && char_len < min_chars) {
                return; // 다음 섹션과 합쳐질 때까지 보류
            }
            sections.push_back({current_heading, current_heading_path, current_section});
            current_section.clear();
        };

        while (std::getline(stream, line)) {
            if (!in_fence && is_heading_line(line)) {
                flush(false);

                const std::size_t level = heading_level(line);
                if (level <= heading_stack.size()) {
                    heading_stack.resize(level - 1);
                }
                heading_stack.push_back(extract_heading_title(line));
                current_heading = heading_stack.back();
                current_heading_path = join_heading_path(heading_stack);
                current_section += line + "\n";
                continue;
            }

            if (is_fence_marker(line)) {
                in_fence = !in_fence;
            }
            current_section += line + "\n";
        }
        flush(true);

        return sections;
    }

    static auto is_fence_marker(const std::string &line) -> bool {
        return line.rfind("```", 0) == 0 || line.rfind("~~~", 0) == 0;
    }

    static auto heading_level(const std::string &line) -> std::size_t {
        std::size_t i = 0;
        while (i < line.size() && line[i] == '#') {
            ++i;
        }
        return i;
    }

    static auto is_heading_line(const std::string &line) -> bool {
        if (line.empty() || line[0] != '#') {
            return false;
        }
        const std::size_t i = heading_level(line);
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

    static auto join_heading_path(const std::vector<std::string> &stack) -> std::string {
        std::string result;
        for (std::size_t i = 0; i < stack.size(); ++i) {
            if (i > 0) {
                result += " > ";
            }
            result += stack[i];
        }
        return result;
    }

    std::size_t chunk_size_;
    std::size_t overlap_;
    SimpleChunker fallback_chunker_;
};

} // namespace ragcli::chunking
