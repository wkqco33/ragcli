#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "chunking/chunk.hpp"
#include "chunking/chunker_interface.hpp"
#include "utils/base64.hpp"
#include "utils/image_utils.hpp"
#include "utils/utf8.hpp"

namespace ragcli::chunking {

// 재귀 구분자 기반 텍스트 청커. 문단 > 줄 > 문장 > 공백 순으로 의미 경계를 찾아
// 자르며, 마땅한 경계가 없을 때만 UTF-8 코드포인트 경계로 폴백한다.
inline constexpr std::size_t k_default_chunk_size = 512; // 문자(코드포인트) 수 기준
inline constexpr std::size_t k_default_overlap = 64;     // 문자(코드포인트) 수 기준

// 생성자 인자가 모두 std::size_t라서 순서를 바꿔 전달하기 쉬운 실수를 막기 위해
// chunk size 와 overlap 각각을 강력한 타입으로 구분한다.
struct ChunkSize {
    std::size_t value;
};
struct Overlap {
    std::size_t value;
};

class SimpleChunker : public Chunker {
  public:
    explicit SimpleChunker(ChunkSize chunk_size = ChunkSize{k_default_chunk_size},
                           Overlap overlap = Overlap{k_default_overlap})
        : chunk_size_(chunk_size.value), overlap_(overlap.value) {
        if (overlap_ >= chunk_size_) {
            overlap_ = chunk_size_ / 2;
        }
    }

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
                continue;
            }

            if (page.text.empty()) {
                continue;
            }

            for (const auto &[start, end] : split_boundaries(page.text)) {
                Chunk chunk;
                chunk.text = page.text.substr(start, end - start);
                chunk.title = page.title;
                chunk.source = source;
                chunk.page_index = page.page_index;
                chunk.chunk_index = chunk_index++;
                chunks.push_back(std::move(chunk));
            }
        }

        return chunks;
    }

  private:
    // 페이지 텍스트를 [start, end) 바이트 범위 목록으로 분할한다. 모든 경계는
    // UTF-8 코드포인트 경계임이 보장된다.
    auto split_boundaries(const std::string &text) const
        -> std::vector<std::pair<std::size_t, std::size_t>> {
        std::vector<std::pair<std::size_t, std::size_t>> boundaries;
        const std::size_t total_bytes = text.size();
        std::size_t start = 0;

        while (start < total_bytes) {
            std::size_t window_end = utils::utf8::advance_chars(text, start, chunk_size_);
            std::size_t end = window_end;

            if (window_end < total_bytes) {
                end = find_semantic_boundary(text, start, window_end);
                if (end <= start) {
                    end = utils::utf8::snap_back(text, window_end);
                }
            }

            boundaries.emplace_back(start, end);
            if (end >= total_bytes) {
                break;
            }

            std::size_t next_start = utils::utf8::retreat_chars(text, end, overlap_);
            if (next_start <= start) {
                // 경계 보정으로 인해 진행이 없거나 역행하면 오버랩 없이 강제 전진한다.
                next_start = end;
            }
            start = next_start;
        }

        merge_small_tail(text, boundaries);
        return boundaries;
    }

    // 마지막 조각이 chunk_size_/4 문자 미만이면 앞 조각에 흡수한다 (조각 청크 방지).
    void merge_small_tail(const std::string &text,
                          std::vector<std::pair<std::size_t, std::size_t>> &boundaries) const {
        if (boundaries.size() <= 1) {
            return;
        }
        const std::size_t min_tail_chars = (std::max<std::size_t>)(chunk_size_ / 4, 1);
        auto &last = boundaries.back();
        const std::size_t tail_chars =
            utils::utf8::char_count(text, last.second) - utils::utf8::char_count(text, last.first);
        if (tail_chars < min_tail_chars) {
            auto &prev = boundaries[boundaries.size() - 2];
            prev.second = last.second;
            boundaries.pop_back();
        }
    }

    // [start, window_end) 구간의 뒤쪽 lookback 범위 안에서 의미 경계를 찾는다.
    // 우선순위: 문단(\n\n) > 줄(\n) > 문장 종결부호 > 공백/탭 > 폴백(코드포인트 스냅백).
    auto find_semantic_boundary(const std::string &text, std::size_t start,
                                std::size_t window_end) const -> std::size_t {
        if (window_end <= start) {
            return utils::utf8::snap_back(text, window_end);
        }

        const std::size_t max_lookback_chars = (std::max<std::size_t>)(chunk_size_ / 4, 1);
        std::size_t lookback_start = utils::utf8::retreat_chars(text, window_end, max_lookback_chars);
        if (lookback_start < start) {
            lookback_start = start;
        }

        const std::string_view window(text.data() + lookback_start, window_end - lookback_start);

        if (auto pos = window.rfind("\n\n"); pos != std::string_view::npos) {
            return lookback_start + pos + 2;
        }
        if (auto pos = window.rfind('\n'); pos != std::string_view::npos) {
            return lookback_start + pos + 1;
        }

        // 문장 종결부호 목록 (마침표/느낌표/물음표 + 한중일 전각형).
        static constexpr std::array<std::string_view, 6> k_enders = {
            ".", "!", "?", "\xE3\x80\x82" /* 。 */, "\xEF\xBC\x81" /* ！ */,
            "\xEF\xBC\x9F" /* ？ */};

        std::size_t best_pos = std::string_view::npos;
        std::size_t best_len = 0;
        for (const auto &ender : k_enders) {
            auto pos = window.rfind(ender);
            if (pos == std::string_view::npos) {
                continue;
            }
            const std::size_t after = pos + ender.size();
            const bool boundary_ok =
                after >= window.size() || window[after] == ' ' || window[after] == '\n' ||
                window[after] == '\t';
            if (!boundary_ok) {
                continue;
            }
            if (best_pos == std::string_view::npos || pos > best_pos) {
                best_pos = pos;
                best_len = ender.size();
            }
        }
        if (best_pos != std::string_view::npos) {
            std::size_t after = lookback_start + best_pos + best_len;
            if (after < text.size() && text[after] == ' ') {
                ++after;
            }
            return after;
        }

        if (auto pos = window.find_last_of(" \t"); pos != std::string_view::npos) {
            return lookback_start + pos + 1;
        }

        return utils::utf8::snap_back(text, window_end);
    }

    std::size_t chunk_size_;
    std::size_t overlap_;
};

} // namespace ragcli::chunking
