#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "qdrant/qdrant_client.hpp"

namespace ragcli::rag {

// RAG 로직에서 필요한 Qdrant 기능만 노출하는 포트.
class QdrantPort {
  public:
    virtual ~QdrantPort() = default;

    virtual auto search(const std::vector<float> &query_vec, int limit,
                        double score_threshold = 0.0, const std::string &vector_name = "") const
        -> std::vector<qdrant::QdrantClient::SearchResultItem> = 0;

    virtual auto create_collection(int vector_size, const std::string &distance = "Cosine") const
        -> void = 0;

    virtual auto upsert_point(const std::vector<float> &embedding,
                              const qdrant::QdrantClient::PointData &data) const -> void = 0;
};

// 실제 qdrant::QdrantClient 를 QdrantPort 에 적응시킨 어댑터.
class QdrantClientAdapter : public QdrantPort {
  public:
    explicit QdrantClientAdapter(std::shared_ptr<qdrant::QdrantClient> client)
        : client_(std::move(client)) {}

    auto search(const std::vector<float> &query_vec, int limit, double score_threshold,
                const std::string &vector_name) const
        -> std::vector<qdrant::QdrantClient::SearchResultItem> override {
        return client_->search(query_vec, limit, score_threshold, vector_name);
    }

    auto create_collection(int vector_size, const std::string &distance) const -> void override {
        client_->create_collection(vector_size, distance);
    }

    auto upsert_point(const std::vector<float> &embedding,
                      const qdrant::QdrantClient::PointData &data) const -> void override {
        client_->upsert_point(embedding, data);
    }

  private:
    std::shared_ptr<qdrant::QdrantClient> client_;
};

} // namespace ragcli::rag
