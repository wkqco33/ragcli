#pragma once

#include <string>
#include <vector>

#include "llm_client/llm_client_interface.hpp"

namespace ragcli::embedding {

// 텍스트 목록을 임베딩 벡터로 변환하는 추상화.
class EmbeddingProvider {
  public:
    virtual ~EmbeddingProvider() = default;

    // texts 의 각 항목에 대한 임베딩을 반환한다.
    virtual auto embed(const std::vector<std::string> &texts, const std::string &model) const
        -> std::vector<std::vector<float>> = 0;
};

} // namespace ragcli::embedding
