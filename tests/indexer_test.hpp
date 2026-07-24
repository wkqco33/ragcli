#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "document/document_source.hpp"
#include "embedding/embedding_provider.hpp"

namespace ragcli::test {

class MockDocumentSource : public document::DocumentSource {
  public:
    explicit MockDocumentSource(std::vector<document::ExtractedPage> pages,
                                std::string name = "mock")
        : pages_(std::move(pages)), name_(std::move(name)) {}

    auto extract() const -> std::vector<document::ExtractedPage> override {
        return pages_;
    }

    auto source_name() const -> std::string override {
        return name_;
    }

  private:
    std::vector<document::ExtractedPage> pages_;
    std::string name_;
};

class MockEmbeddingProvider : public embedding::EmbeddingProvider {
  public:
    auto embed(const std::vector<std::string> &texts, const std::string & /*model*/) const
        -> std::vector<std::vector<float>> override {
        std::vector<std::vector<float>> result;
        result.reserve(texts.size());
        for (std::size_t i = 0; i < texts.size(); ++i) {
            // 텍스트 길이를 반영한 deterministic 임베딩.
            result.push_back(std::vector<float>(128, static_cast<float>(texts[i].size())));
        }
        return result;
    }
};

} // namespace ragcli::test
