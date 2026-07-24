#pragma once

#include <vector>

#include "chunking/chunk.hpp"
#include "document/document_source.hpp"

namespace ragcli::chunking {

// DocumentSource 로부터 추출된 페이지들을 Chunk 목록으로 변환한다.
class Chunker {
  public:
    virtual ~Chunker() = default;

    virtual auto chunk(const std::vector<document::ExtractedPage> &pages,
                       const std::string &source_name) const -> std::vector<Chunk> = 0;
};

} // namespace ragcli::chunking
