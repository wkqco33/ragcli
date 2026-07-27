#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "qdrant/qdrant_client.hpp"

namespace ragcli::rag {

// QdrantPort 검색 결과 한 건. qdrant::QdrantClient::SearchResultItem 과 필드가
// 같지만, RAG 로직이 Qdrant 전용 타입에 직접 의존하지 않도록 분리했다 (다른
// 벡터 스토어로 교체할 때 이 구조체만 채우면 된다).
struct SearchHit {
    std::string text;
    double score = 0.0;
    bool is_image = false;
    std::string image_base64;
};

// QdrantPort::upsert_point 에 전달하는 페이로드. qdrant::QdrantClient::PointData
// 와 필드가 같지만 위와 같은 이유로 분리했다.
struct UpsertPoint {
    std::string content;
    std::string title;
    std::string source;      // 출처 파일 경로
    std::string source_type; // ragcli_add / ragcli_index 등
    int page_index = 0;      // 페이지 번호
    int chunk_index = 0;     // 청크 번호
    bool is_image = false;   // 이미지 콘텐츠 여부
    int image_width = 0;
    int image_height = 0;
    std::string image_base64;
};

// RAG 로직에서 필요한 Qdrant 기능만 노출하는 포트.
// search/upsert_point 는 SearchHit/UpsertPoint 라는 벡터 스토어 중립 타입만
// 주고받으므로, 다른 벡터 DB로 교체하려면 이 인터페이스의 새 구현체만 추가하면
// 된다 (qdrant::QdrantClient 의 타입이 RAG/Indexer 쪽 코드로 새어나가지 않는다).
class QdrantPort {
  public:
    virtual ~QdrantPort() = default;

    virtual auto search(const std::vector<float> &query_vec, int limit,
                        double score_threshold = 0.0,
                        const std::string &vector_name = "") const -> std::vector<SearchHit> = 0;

    virtual auto create_collection(int vector_size,
                                   const std::string &distance = "Cosine") const -> void = 0;

    virtual auto upsert_point(const std::vector<float> &embedding,
                              const UpsertPoint &data) const -> void = 0;
};

// 실제 qdrant::QdrantClient 를 QdrantPort 에 적응시킨 어댑터.
// 중립 타입(SearchHit/UpsertPoint) <-> Qdrant 전용 타입 변환은 이 클래스 안에서만 일어난다.
class QdrantClientAdapter : public QdrantPort {
  public:
    explicit QdrantClientAdapter(std::shared_ptr<qdrant::QdrantClient> client)
        : client_(std::move(client)) {}

    auto search(const std::vector<float> &query_vec, int limit, double score_threshold,
                const std::string &vector_name) const -> std::vector<SearchHit> override {
        auto items = client_->search(query_vec, limit, score_threshold, vector_name);

        std::vector<SearchHit> hits;
        hits.reserve(items.size());
        for (auto &item : items) {
            hits.push_back(SearchHit{std::move(item.text), item.score, item.is_image,
                                     std::move(item.image_base64)});
        }
        return hits;
    }

    auto create_collection(int vector_size, const std::string &distance) const -> void override {
        client_->create_collection(vector_size, distance);
    }

    auto upsert_point(const std::vector<float> &embedding,
                      const UpsertPoint &data) const -> void override {
        qdrant::QdrantClient::PointData point_data;
        point_data.content = data.content;
        point_data.title = data.title;
        point_data.source = data.source;
        point_data.source_type = data.source_type;
        point_data.page_index = data.page_index;
        point_data.chunk_index = data.chunk_index;
        point_data.is_image = data.is_image;
        point_data.image_width = data.image_width;
        point_data.image_height = data.image_height;
        point_data.image_base64 = data.image_base64;

        client_->upsert_point(embedding, point_data);
    }

  private:
    std::shared_ptr<qdrant::QdrantClient> client_;
};

} // namespace ragcli::rag
