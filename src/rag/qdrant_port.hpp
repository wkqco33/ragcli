#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "qdrant/qdrant_client.hpp"

namespace ragcli::rag {

// QdrantPort 검색 결과 한 건 (qdrant::QdrantClient::SearchResultItem 과 동일 필드).
struct SearchHit {
    std::string text;
    std::string title;
    std::string source;       // 출처 파일 경로
    std::string heading_path; // 마크다운 헤딩 계층 경로 (없으면 빈 문자열)
    int page_index = 0;
    int chunk_index = 0;
    int chunk_total = 0;
    double score = 0.0;
    bool is_image = false;
    std::string image_base64;
};

// QdrantPort::upsert_point 에 전달하는 페이로드 (qdrant::QdrantClient::PointData 와 동일 필드).
struct UpsertPoint {
    std::string content;
    std::string title;
    std::string source;       // 출처 파일 경로
    std::string source_type;  // ragcli_add / ragcli_index 등
    std::string heading_path; // 마크다운 헤딩 계층 경로 (없으면 빈 문자열)
    int page_index = 0;       // 페이지 번호
    int chunk_index = 0;      // 청크 번호
    int chunk_total = 0;      // 같은 소스 내 총 청크 수
    bool is_image = false;    // 이미지 콘텐츠 여부
    int image_width = 0;
    int image_height = 0;
    std::string image_base64;
};

// RAG 로직에서 필요한 Qdrant 기능만 노출하는 포트. 벡터 스토어 중립 타입만 주고받으므로
// 다른 벡터 DB로 교체하려면 이 인터페이스의 새 구현체만 추가하면 된다.
class QdrantPort {
  public:
    virtual ~QdrantPort() = default;

    virtual auto search(const std::vector<float> &query_vec, int limit,
                        double score_threshold = 0.0,
                        const std::string &vector_name = "") const -> std::vector<SearchHit> = 0;

    // 같은 source 안에서 chunk_index 가 [min_index, max_index] 범위인 이웃 청크를 가져온다.
    // 벡터 검색이 아니므로 반환된 SearchHit::score 는 항상 0.0 이다.
    virtual auto fetch_neighbors(const std::string &source, int min_index,
                                 int max_index) const -> std::vector<SearchHit> = 0;

    virtual auto create_collection(int vector_size,
                                   const std::string &distance = "Cosine") const -> void = 0;

    virtual auto upsert_point(const std::vector<float> &embedding,
                              const UpsertPoint &data) const -> void = 0;
};

// 실제 qdrant::QdrantClient 를 QdrantPort 에 적응시키는 어댑터 (중립 타입 변환은 여기서만).
class QdrantClientAdapter : public QdrantPort {
  public:
    explicit QdrantClientAdapter(std::shared_ptr<qdrant::QdrantClient> client)
        : client_(std::move(client)) {}

    auto search(const std::vector<float> &query_vec, int limit, double score_threshold,
                const std::string &vector_name) const -> std::vector<SearchHit> override {
        auto items = client_->search(query_vec, limit, score_threshold, vector_name);
        return to_hits(items);
    }

    auto fetch_neighbors(const std::string &source, int min_index,
                         int max_index) const -> std::vector<SearchHit> override {
        auto items = client_->scroll_by_chunk_range(source, min_index, max_index);
        return to_hits(items);
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
        point_data.heading_path = data.heading_path;
        point_data.page_index = data.page_index;
        point_data.chunk_index = data.chunk_index;
        point_data.chunk_total = data.chunk_total;
        point_data.is_image = data.is_image;
        point_data.image_width = data.image_width;
        point_data.image_height = data.image_height;
        point_data.image_base64 = data.image_base64;

        client_->upsert_point(embedding, point_data);
    }

  private:
    static auto
    to_hits(std::vector<qdrant::QdrantClient::SearchResultItem> &items) -> std::vector<SearchHit> {
        std::vector<SearchHit> hits;
        hits.reserve(items.size());
        for (auto &item : items) {
            SearchHit hit;
            hit.text = std::move(item.text);
            hit.title = std::move(item.title);
            hit.source = std::move(item.source);
            hit.heading_path = std::move(item.heading_path);
            hit.page_index = item.page_index;
            hit.chunk_index = item.chunk_index;
            hit.chunk_total = item.chunk_total;
            hit.score = item.score;
            hit.is_image = item.is_image;
            hit.image_base64 = std::move(item.image_base64);
            hits.push_back(std::move(hit));
        }
        return hits;
    }

    std::shared_ptr<qdrant::QdrantClient> client_;
};

} // namespace ragcli::rag
