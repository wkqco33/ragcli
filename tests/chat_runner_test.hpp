#pragma once

#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "llm_client/llm_client_interface.hpp"

namespace ragcli::test {

class MockLlmClient : public llm_client::LLMClientInterface {
  public:
    auto chat(const std::vector<llm_client::Message> &messages,
              const llm_client::RequestParams &params) -> llm_client::ResponseData override {
        (void)params;
        last_messages_ = messages;
        llm_client::ResponseData response;
        response.content =
            "mock reply to: " + (messages.empty() ? std::string() : messages.back().content);
        response.model = params.model;
        return response;
    }

    auto chatStream(const std::vector<llm_client::Message> & /*messages*/,
                    llm_client::StreamCallback /*callback*/,
                    const llm_client::RequestParams & /*params*/)
        -> llm_client::ResponseData override {
        return {};
    }

    std::vector<llm_client::Message> last_messages_;
};

// 테스트에서 사용할 입력 시퀀스를 관리하는 팩토리 함수.
inline auto make_input_provider(std::queue<std::string> &inputs, bool *eof_reached = nullptr) {
    return [&inputs, eof_reached](std::string &out) {
        if (inputs.empty()) {
            if (eof_reached != nullptr) {
                *eof_reached = true;
            }
            out.clear();
            return false;
        }
        out = std::move(inputs.front());
        inputs.pop();
        return true;
    };
}

class MockLlmClientWithError : public MockLlmClient {
  public:
    auto chat(const std::vector<llm_client::Message> &messages,
              const llm_client::RequestParams &params) -> llm_client::ResponseData override {
        if (params.model == "throw") {
            throw std::runtime_error("forced error");
        }
        return MockLlmClient::chat(messages, params);
    }
};

} // namespace ragcli::test
