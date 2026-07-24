#pragma once

#include <string>
#include <vector>

#include "llm_client/llm_client_interface.hpp"

namespace ragcli::rag {

// RAG 로직에서 필요한 LLM 기능만 노출하는 포트.
class LlmPort {
  public:
    virtual ~LlmPort() = default;

    virtual auto embed(const std::vector<std::string> &inputs,
                       const llm_client::EmbeddingParams &params) const
        -> llm_client::EmbeddingResponse = 0;

    virtual auto generate(const std::string &prompt, const llm_client::RequestParams &params) const
        -> llm_client::ResponseData = 0;

    virtual auto chat(const std::vector<llm_client::Message> &messages,
                      const llm_client::RequestParams &params) const
        -> llm_client::ResponseData = 0;
};

// 실제 llm_client::LLMClientInterface 구현체를 LlmPort 에 적응시킨 어댑터.
class LlmClientAdapter : public LlmPort {
  public:
    explicit LlmClientAdapter(std::shared_ptr<llm_client::LLMClientInterface> client)
        : client_(std::move(client)) {}

    auto embed(const std::vector<std::string> &inputs,
               const llm_client::EmbeddingParams &params) const
        -> llm_client::EmbeddingResponse override {
        return client_->embed(inputs, params);
    }

    auto generate(const std::string &prompt, const llm_client::RequestParams &params) const
        -> llm_client::ResponseData override {
        return client_->generate(prompt, params);
    }

    auto chat(const std::vector<llm_client::Message> &messages,
              const llm_client::RequestParams &params) const
        -> llm_client::ResponseData override {
        return client_->chat(messages, params);
    }

  private:
    std::shared_ptr<llm_client::LLMClientInterface> client_;
};

} // namespace ragcli::rag
