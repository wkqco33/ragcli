#pragma once

#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "chunking/chunk.hpp"
#include "chunking/chunker_interface.hpp"
#include "chunking/markdown_chunker.hpp"
#include "chunking/simple_chunker.hpp"

namespace ragcli::chunking {

// 페이지의 소스 파일 확장자를 보고 Markdown 페이지는 MarkdownChunker, 그 외에는
// SimpleChunker 로 라우팅한다. 디렉터리 인덱싱처럼 여러 파일이 섞인 입력에서도
// 파일별로 적절한 청킹 전략이 자동 적용된다.
class AutoChunker : public Chunker {
  public:
    explicit AutoChunker(ChunkSize chunk_size = ChunkSize{k_default_chunk_size},
                         Overlap overlap = Overlap{k_default_overlap})
        : markdown_chunker_(chunk_size, overlap), simple_chunker_(chunk_size, overlap) {}

    auto chunk(const std::vector<document::ExtractedPage> &pages,
               const std::string &source_name) const -> std::vector<Chunk> override {
        std::vector<Chunk> chunks;
        int chunk_index = 0;

        std::vector<document::ExtractedPage> run;
        bool run_is_markdown = false;
        bool run_started = false;

        auto flush_run = [&]() {
            if (run.empty()) {
                return;
            }
            auto sub_chunks = run_is_markdown ? markdown_chunker_.chunk(run, source_name)
                                              : simple_chunker_.chunk(run, source_name);
            for (auto &sub_chunk : sub_chunks) {
                sub_chunk.chunk_index = chunk_index++;
                chunks.push_back(std::move(sub_chunk));
            }
            run.clear();
        };

        for (const auto &page : pages) {
            const bool is_md = is_markdown_page(page);
            if (run_started && is_md != run_is_markdown) {
                flush_run();
            }
            run_is_markdown = is_md;
            run_started = true;
            run.push_back(page);
        }
        flush_run();

        return chunks;
    }

  private:
    static auto is_markdown_page(const document::ExtractedPage &page) -> bool {
        const std::string &path = !page.source_path.empty() ? page.source_path : page.title;
        const std::size_t dot_pos = path.rfind('.');
        if (dot_pos == std::string::npos) {
            return false;
        }
        std::string ext = path.substr(dot_pos);
        for (char &c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return ext == ".md" || ext == ".markdown";
    }

    MarkdownChunker markdown_chunker_;
    SimpleChunker simple_chunker_;
};

} // namespace ragcli::chunking
