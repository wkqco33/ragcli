#pragma once

#include <string>
#include <utility>
#include <vector>

#include "rag/qdrant_port.hpp"

namespace ragcli::test {

// 테스트에서 재사용하는 QdrantPort Mock.
class MockQdrantPort : public rag::QdrantPort {
  public:
    auto search(const std::vector<float> & /*query_vec*/, int limit, double /*score_threshold*/,
                const std::string & /*vector_name*/) const -> std::vector<rag::SearchHit> override {
        (void)limit;
        return search_results_;
    }

    auto create_collection(int /*vector_size*/, const std::string & /*distance*/) const
        -> void override {
        collection_created_ = true;
    }

    auto upsert_point(const std::vector<float> & /*embedding*/, const rag::UpsertPoint &data) const
        -> void override {
        last_upsert_content_ = data.content;
        last_upsert_title_ = data.title;
        last_data_.push_back(data);
    }

    mutable std::string last_upsert_content_;
    mutable std::string last_upsert_title_;
    mutable std::vector<rag::UpsertPoint> last_data_;
    mutable std::vector<rag::SearchHit> search_results_;
    mutable bool collection_created_ = false;
};

} // namespace ragcli::test
