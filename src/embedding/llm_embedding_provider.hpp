#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "embedding/embedding_provider.hpp"
#include "llm_client/llm_client_interface.hpp"
#include "rag/llm_port.hpp"

namespace ragcli::embedding {

// llm_client 기반 EmbeddingProvider 구현.
class LlmEmbeddingProvider : public EmbeddingProvider {
  public:
    explicit LlmEmbeddingProvider(std::shared_ptr<rag::LlmPort> llm_port)
        : llm_port_(std::move(llm_port)) {}

    auto embed(const std::vector<std::string> &texts,
               const std::string &model) const -> std::vector<std::vector<float>> override {
        llm_client::EmbeddingParams params;
        params.model = model;

        auto response = llm_port_->embed(texts, params);
        return response.embeddings;
    }

  private:
    std::shared_ptr<rag::LlmPort> llm_port_;
};

} // namespace ragcli::embedding
