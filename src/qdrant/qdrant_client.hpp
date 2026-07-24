#pragma once

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>
#include <wcppcli/wlog.hpp>

#include "utils/uuid.hpp"

namespace ragcli::qdrant {

using json = nlohmann::json;

// Qdrant REST API 클라이언트.
class QdrantClient {
  public:
    explicit QdrantClient(std::string base_url, std::string collection_name)
        : base_url_(std::move(base_url)), collection_name_(std::move(collection_name)) {}

    // 컬렉션 기본 벡터 설정을 조회한다 (차원, 거리 함수 등).
    [[nodiscard]] auto get_collection_info() const -> json {
        const std::string url = base_url_ + "/collections/" + collection_name_;
        cpr::Response response = cpr::Get(cpr::Url{url});

        verify_response(response, "Qdrant get collection info failed");
        return parse_json_safely(response.text, "Qdrant collection info");
    }

    // 모든 컬렉션 목록을 조회한다.
    [[nodiscard]] auto list_collections() const -> json {
        const std::string url = base_url_ + "/collections";
        cpr::Response response = cpr::Get(cpr::Url{url});

        verify_response(response, "Qdrant list collections failed");
        return parse_json_safely(response.text, "Qdrant collections list");
    }

    // 컬렉션을 삭제한다.
    auto delete_collection() const -> void {
        const std::string url = base_url_ + "/collections/" + collection_name_;
        cpr::Response response = cpr::Delete(cpr::Url{url});

        verify_response(response, "Qdrant delete collection failed");
        wcppcli::WLog::info("Deleted Qdrant collection '" + collection_name_ + "'");
    }

    // 컬렉션을 생성한다 (이미 존재하면 Qdrant 가 OK 를 반환).
    auto create_collection(int vector_size, const std::string &distance = "Cosine") const -> void {
        const std::string url = base_url_ + "/collections/" + collection_name_;
        json req;
        req["vectors"]["size"] = vector_size;
        req["vectors"]["distance"] = distance;

        cpr::Header headers{{"Content-Type", "application/json"}};
        cpr::Response response = cpr::Put(cpr::Url{url}, headers, cpr::Body{req.dump()});

        verify_response(response, "Qdrant create collection failed");
        wcppcli::WLog::info("Created Qdrant collection '" + collection_name_ + "' with size " +
                            std::to_string(vector_size));
    }

    struct SearchResultItem {
        std::string text;
        double score = 0.0;
        bool is_image = false;
        std::string image_base64;
    };

    // query_vec 와 가장 가까운 k 개의 페이로드를 점수순으로 반환한다.
    [[nodiscard]] auto search(const std::vector<float> &query_vec, int limit,
                              double score_threshold = 0.0,
                              const std::string &vector_name = "") const
        -> std::vector<SearchResultItem> {
        if (limit <= 0) {
            return {};
        }

        const std::string url = base_url_ + "/collections/" + collection_name_ + "/points/search";
        json req = build_search_request(query_vec, {limit, score_threshold, vector_name});

        cpr::Response response = execute_search_request(url, req.dump());
        return parse_search_response(parse_json_safely(response.text, "Qdrant search response"));
    }

    struct PointData {
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

    // 텍스트, 제목 및 임베딩 벡터를 Qdrant 컬렉션에 추가(Upsert)한다.
    auto upsert_point(const std::vector<float> &embedding, const PointData &data) const -> void {
        const std::string url =
            base_url_ + "/collections/" + collection_name_ + "/points?wait=true";

        json point;
        point["id"] = utils::generate_uuid_v4();
        point["vector"] = embedding;

        json payload;
        payload["content"] = data.content;
        if (!data.title.empty()) {
            payload["title"] = data.title;
        }
        if (!data.source.empty()) {
            payload["source"] = data.source;
        }
        if (!data.source_type.empty()) {
            payload["source_type"] = data.source_type;
        }
        if (data.page_index > 0) {
            payload["page_index"] = data.page_index;
        }
        if (data.chunk_index > 0) {
            payload["chunk_index"] = data.chunk_index;
        }
        if (data.is_image) {
            payload["is_image"] = true;
            payload["image_width"] = data.image_width;
            payload["image_height"] = data.image_height;
            if (!data.image_base64.empty()) {
                payload["image_base64"] = data.image_base64;
            }
        }
        point["payload"] = payload;

        json req;
        req["points"] = json::array({point});

        cpr::Header headers{{"Content-Type", "application/json"}};
        cpr::Response response = cpr::Put(cpr::Url{url}, headers, cpr::Body{req.dump()});

        verify_response(response, "Qdrant upsert failed");
        wcppcli::WLog::info("Successfully added point to Qdrant");
    }

  private:
    static constexpr int k_http_ok = 200;

    static auto verify_response(const cpr::Response &response, const std::string &action_name) -> void {
        if (response.error) {
            throw std::runtime_error(action_name + " due to connection error: " + response.error.message);
        }
        if (response.status_code != k_http_ok) {
            throw std::runtime_error(action_name + " (HTTP " +
                                     std::to_string(response.status_code) + "): " + response.text);
        }
    }

    static auto parse_json_safely(const std::string &text, const std::string &context) -> json {
        if (text.empty()) {
            throw std::runtime_error(context + " returned an empty response.");
        }
        try {
            return json::parse(text);
        } catch (const json::parse_error &e) {
            throw std::runtime_error(context + " invalid JSON format: " + std::string(e.what()));
        }
    }

    struct SearchOptions {
        int limit;
        double score_threshold;
        std::string vector_name;
    };

    static auto execute_search_request(const std::string &url, const std::string &body)
        -> cpr::Response {
        cpr::Header headers{{"Content-Type", "application/json"}};
        cpr::Response response = cpr::Post(cpr::Url{url}, headers, cpr::Body{body});
        verify_response(response, "Qdrant search failed");
        return response;
    }

    static auto parse_search_response(const json &res)
        -> std::vector<SearchResultItem> {
        std::vector<SearchResultItem> results;

        if (!res.contains("result") || !res["result"].is_array()) {
            wcppcli::WLog::warn("Qdrant response has no 'result' array");
            return results;
        }

        results.reserve(res["result"].size());

        for (const auto &item : res["result"]) {
            std::string text = extract_text_from_item(item);
            if (text.empty()) {
                continue;
            }

            SearchResultItem sitem;
            sitem.text = std::move(text);
            sitem.score = extract_score_from_item(item);

            if (item.contains("payload") && item["payload"].is_object()) {
                const auto &payload = item["payload"];
                if (payload.contains("is_image") && payload["is_image"].is_boolean()) {
                    sitem.is_image = payload["is_image"].get<bool>();
                }
                if (payload.contains("image_base64") && payload["image_base64"].is_string()) {
                    sitem.image_base64 = payload["image_base64"].get<std::string>();
                }
            }

            results.push_back(std::move(sitem));
        }

        return results;
    }

    static auto extract_text_from_item(const json &item) -> std::string {
        if (!item.contains("payload") || !item["payload"].is_object()) {
            return {};
        }

        const auto &payload = item["payload"];
        std::string title = extract_string_field(payload, "title");
        std::string body = extract_body_field(payload);

        if (!title.empty() && !body.empty()) {
            return "[제목] " + std::move(title) + "\n[내용] " + std::move(body);
        }
        if (!body.empty()) {
            return body;
        }
        return title;
    }

    static auto extract_body_field(const json &payload) -> std::string {
        for (const char *field : {"content", "text", "document", "body"}) {
            std::string value = extract_string_field(payload, field);
            if (!value.empty()) {
                return value;
            }
        }
        return {};
    }

    static auto extract_string_field(const json &obj, const char *key) -> std::string {
        if (obj.contains(key) && obj[key].is_string()) {
            return obj[key].get<std::string>();
        }
        return {};
    }

    static auto extract_score_from_item(const json &item) -> double {
        if (item.contains("score") && item["score"].is_number()) {
            return item["score"].get<double>();
        }
        return 0.0;
    }

    static auto build_search_request(const std::vector<float> &query_vec,
                                     const SearchOptions &options) -> json {
        json req;
        req["vector"] = query_vec;
        req["limit"] = options.limit;
        req["with_payload"] = true;

        if (options.score_threshold > 0.0) {
            req["score_threshold"] = options.score_threshold;
        }

        if (!options.vector_name.empty()) {
            req["using"] = options.vector_name;
        }

        return req;
    }

    std::string base_url_;
    std::string collection_name_;
};

} // namespace ragcli::qdrant
