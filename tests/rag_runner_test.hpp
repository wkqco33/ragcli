#pragma once

#include <string>
#include <utility>
#include <vector>

#include "llm_client/llm_client_interface.hpp"
#include "rag/llm_port.hpp"

namespace ragcli::test {

class MockLlmPort : public rag::LlmPort {
  public:
    auto embed(const std::vector<std::string> & /*inputs*/,
               const llm_client::EmbeddingParams & /*params*/) const
        -> llm_client::EmbeddingResponse override {
        llm_client::EmbeddingResponse response;
        if (embeddings_returned_) {
            response.embeddings = {std::vector<float>(128, 0.01F)};
        }
        response.model = "mock-embed-model";
        return response;
    }

    auto generate(const std::string & /*prompt*/, const llm_client::RequestParams &params) const
        -> llm_client::ResponseData override {
        llm_client::ResponseData response;
        response.content = "mock answer with model " + params.model;
        response.model = params.model;
        return response;
    }

    auto chat(const std::vector<llm_client::Message> & /*messages*/,
              const llm_client::RequestParams &params) const
        -> llm_client::ResponseData override {
        llm_client::ResponseData response;
        response.content = "mock vision answer with model " + params.model;
        response.model = params.model;
        return response;
    }

    void set_embeddings_returned(bool returned) {
        embeddings_returned_ = returned;
    }

  private:
    bool embeddings_returned_ = true;
};

} // namespace ragcli::test
