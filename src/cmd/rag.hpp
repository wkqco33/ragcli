#pragma once

#include <cpr/cpr.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "command.hpp"
#include "llm_client/llm_client_factory.hpp"
#include "wcppcli/wconf.hpp"

namespace ragcli::cmd {

using json = nlohmann::json;

// UUID v4 무작위 문자열 생성 유틸리티
inline auto generate_uuid_v4() -> std::string {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis(0, 15);
    static std::uniform_int_distribution<uint32_t> dis2(8, 11);

    const char *hex = "0123456789abcdef";
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (char &c : uuid) {
        if (c == 'x') {
            c = hex[dis(gen)];
        } else if (c == 'y') {
            c = hex[dis2(gen)];
        }
    }
    return uuid;
}

// Qdrant REST API 클라이언트.
class QdrantClient {
  public:
    explicit QdrantClient(std::string base_url, std::string collection_name)
        : base_url_(std::move(base_url)), collection_name_(std::move(collection_name)) {}

    // 컬렉션 기본 벡터 설정을 조회한다 (차원, 거리 함수 등).
    [[nodiscard]] auto get_collection_info() const -> json {
        const std::string url = base_url_ + "/collections/" + collection_name_;
        cpr::Response response = cpr::Get(cpr::Url{url});

        if (response.status_code != http_ok) {
            throw std::runtime_error("Qdrant get collection info failed (HTTP " +
                                     std::to_string(response.status_code) + "): " + response.text);
        }
        return json::parse(response.text);
    }

    // query_vec 와 가장 가까운 k 개의 페이로드를 점수순으로 반환한다.
    [[nodiscard]] auto search(
        const std::vector<float> &query_vec, int limit, double score_threshold = 0.0,
        const std::string &vector_name = "") const -> std::vector<std::pair<std::string, double>> {
        if (limit <= 0) {
            return {};
        }

        const std::string url = base_url_ + "/collections/" + collection_name_ + "/points/search";
        json req = build_search_request(query_vec, {limit, score_threshold, vector_name});

        cpr::Response response = execute_search_request(url, req.dump());
        return parse_search_response(json::parse(response.text));
    }

    // 텍스트, 제목 및 임베딩 벡터를 Qdrant 컬렉션에 추가(Upsert)한다.
    auto upsert_point(const std::vector<float> &embedding, const std::string &content,
                     const std::string &title = "") const -> void {
        const std::string url = base_url_ + "/collections/" + collection_name_ + "/points?wait=true";

        json point;
        point["id"] = generate_uuid_v4();
        point["vector"] = embedding;

        json payload;
        payload["content"] = content;
        if (!title.empty()) {
            payload["title"] = title;
        }
        payload["source_type"] = "ragcli_add";
        point["payload"] = payload;

        json req;
        req["points"] = json::array({point});

        cpr::Header headers{{"Content-Type", "application/json"}};
        wcppcli::WLog::info("Qdrant upsert URL: " + url);

        cpr::Response response = cpr::Put(cpr::Url{url}, headers, cpr::Body{req.dump()});

        if (response.status_code != http_ok) {
            throw std::runtime_error("Qdrant upsert failed (HTTP " +
                                     std::to_string(response.status_code) + "): " + response.text);
        }
        wcppcli::WLog::info("Successfully added point to Qdrant");
    }

  private:
    static constexpr int http_ok = 200;

    struct SearchOptions {
        int limit;
        double score_threshold;
        std::string vector_name;
    };

    static auto execute_search_request(const std::string &url,
                                       const std::string &body) -> cpr::Response {
        cpr::Header headers{{"Content-Type", "application/json"}};
        wcppcli::WLog::info("Qdrant search URL: " + url);
        wcppcli::WLog::info("Qdrant search request: " + body);

        cpr::Response response = cpr::Post(cpr::Url{url}, headers, cpr::Body{body});

        wcppcli::WLog::info("Qdrant search response HTTP " + std::to_string(response.status_code));
        if (!response.text.empty()) {
            wcppcli::WLog::info("Qdrant search response body: " + response.text);
        }

        if (response.status_code != http_ok) {
            throw std::runtime_error("Qdrant search failed (HTTP " +
                                     std::to_string(response.status_code) + "): " + response.text);
        }

        return response;
    }

    static auto
    parse_search_response(const json &res) -> std::vector<std::pair<std::string, double>> {
        std::vector<std::pair<std::string, double>> results;

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

            double score = extract_score_from_item(item);
            results.emplace_back(text, score);
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

// `ragcli rag` 서브커맨드.
// 질문을 임베딩한 뒤 Qdrant 에서 유사 문서를 검색하고, 검색 결과를 컨텍스트로
// LLM 에게 전달해 답변을 생성하거나 지식을 추가한다.
class RagCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "rag";
        cmd->description = "Run retrieval-augmented generation or add knowledge via Qdrant";

        wcppcli::Flag query_flag;
        query_flag.name = "query";
        query_flag.shorthand = 'q';
        query_flag.description = "User query for RAG search";
        query_flag.value_ptr = &query_;
        cmd->add_flag(query_flag);

        wcppcli::Flag add_flag;
        add_flag.name = "add";
        add_flag.shorthand = 'a';
        add_flag.description = "Knowledge text content to add to Qdrant";
        add_flag.value_ptr = &add_text_;
        cmd->add_flag(add_flag);

        wcppcli::Flag title_flag;
        title_flag.name = "title";
        title_flag.description = "Title of the knowledge chunk to add";
        title_flag.value_ptr = &title_;
        cmd->add_flag(title_flag);

        wcppcli::Flag file_flag;
        file_flag.name = "file";
        file_flag.shorthand = 'f';
        file_flag.description = "File path containing knowledge text to add";
        file_flag.value_ptr = &file_path_;
        cmd->add_flag(file_flag);

        wcppcli::Flag model_flag;
        model_flag.name = "model";
        model_flag.shorthand = 'm';
        model_flag.description = "LLM model name (default: llama3)";
        model_flag.value_ptr = &model_;
        cmd->add_flag(model_flag);

        wcppcli::Flag embed_model_flag;
        embed_model_flag.name = "embed-model";
        embed_model_flag.shorthand = 'e';
        embed_model_flag.description = "Embedding model name (default: nomic-embed-text)";
        embed_model_flag.value_ptr = &embed_model_;
        cmd->add_flag(embed_model_flag);

        wcppcli::Flag llm_url_flag;
        llm_url_flag.name = "llm-url";
        llm_url_flag.description = "LLM/Ollama base URL (default: http://localhost:11434)";
        llm_url_flag.value_ptr = &llm_url_;
        cmd->add_flag(llm_url_flag);

        wcppcli::Flag qdrant_url_flag;
        qdrant_url_flag.name = "qdrant-url";
        qdrant_url_flag.description = "Qdrant base URL (default: http://localhost:6333)";
        qdrant_url_flag.value_ptr = &qdrant_url_;
        cmd->add_flag(qdrant_url_flag);

        wcppcli::Flag collection_flag;
        collection_flag.name = "collection";
        collection_flag.shorthand = 'c';
        collection_flag.description = "Qdrant collection name (default: documents)";
        collection_flag.value_ptr = &collection_;
        cmd->add_flag(collection_flag);

        wcppcli::Flag top_k_flag;
        top_k_flag.name = "top-k";
        top_k_flag.shorthand = 'k';
        top_k_flag.description = "Number of retrieved chunks (default: 3)";
        top_k_flag.value_ptr = &top_k_;
        cmd->add_flag(top_k_flag);

        wcppcli::Flag score_threshold_flag;
        score_threshold_flag.name = "score-threshold";
        score_threshold_flag.description =
            "Minimum Qdrant search score 0.0~1.0 (default: 0.0 = disabled)";
        score_threshold_flag.value_ptr = &score_threshold_str_;
        cmd->add_flag(score_threshold_flag);

        cmd->handler = [this](const wcppcli::Command &) { return run_rag(); };

        root.add_command(std::move(cmd));
    }

  private:
    auto run_rag() -> int {
        wcppcli::WConf conf;
        conf.read_file(".env");

        // 설정 우선순위: CLI 플래그 > 환경 변수 > 코드 기본값
        std::string target_llm_url =
            pick_first({&llm_url_, conf.get_string("OLLAMA_BASE_URL"), "http://localhost:11434"});
        std::string target_model = pick_first({&model_, conf.get_string("OLLAMA_MODEL"), "llama3"});
        std::string target_embed_model =
            pick_first({&embed_model_, conf.get_string("OLLAMA_EMBED_MODEL"), "nomic-embed-text"});
        std::string target_qdrant_url =
            pick_first({&qdrant_url_, conf.get_string("QDRANT_BASE_URL"), "http://localhost:6333"});
        std::string target_collection =
            pick_first({&collection_, conf.get_string("QDRANT_COLLECTION"), "documents"});

        // 1) 지식 추가 모드 처리 (-a / --add 또는 -f / --file)
        if (!add_text_.empty() || !file_path_.empty()) {
            return run_add_knowledge(target_llm_url, target_embed_model, target_qdrant_url,
                                       target_collection);
        }

        // 2) 질문 모드 처리 (-q / --query)
        if (!query_.empty()) {
            return run_query_rag(target_llm_url, target_model, target_embed_model,
                                 target_qdrant_url, target_collection);
        }

        wcppcli::WLog::error("Either --query (-q) or --add (-a) / --file (-f) must be specified.");
        return 1;
    }

    auto run_add_knowledge(const std::string &target_llm_url, const std::string &target_embed_model,
                           const std::string &target_qdrant_url,
                           const std::string &target_collection) -> int {
        std::string content_to_add = add_text_;

        if (!file_path_.empty()) {
            std::ifstream file(file_path_);
            if (!file.is_open()) {
                wcppcli::WLog::error("Failed to open file: " + file_path_);
                return 1;
            }
            std::ostringstream ss;
            ss << file.rdbuf();
            content_to_add = ss.str();
        }

        if (content_to_add.empty()) {
            wcppcli::WLog::error("Content to add is empty.");
            return 1;
        }

        try {
            auto llm_client = llm_client::LLMClientFactory::create("ollama", "", target_llm_url);
            QdrantClient qdrant(target_qdrant_url, target_collection);

            wcppcli::WLog::info("Embedding knowledge text (" +
                                std::to_string(content_to_add.size()) + " bytes)...");

            llm_client::EmbeddingParams embed_params;
            embed_params.model = target_embed_model;
            auto embed_res = llm_client->embed({content_to_add}, embed_params);

            if (embed_res.embeddings.empty()) {
                wcppcli::WLog::error("Embedding returned empty result.");
                return 1;
            }

            qdrant.upsert_point(embed_res.embeddings[0], content_to_add, title_);
            wcppcli::WLog::success("Successfully added knowledge to Qdrant collection '" +
                                   target_collection + "'.");
        } catch (const std::exception &e) {
            wcppcli::WLog::error("Add knowledge failed: " + std::string(e.what()));
            return 1;
        }

        return 0;
    }

    auto run_query_rag(const std::string &target_llm_url, const std::string &target_model,
                       const std::string &target_embed_model, const std::string &target_qdrant_url,
                       const std::string &target_collection) -> int {
        int target_top_k = top_k_ > 0 ? top_k_ : 3;
        double target_score_threshold = parse_score_threshold(score_threshold_str_);

        try {
            auto llm_client = llm_client::LLMClientFactory::create("ollama", "", target_llm_url);
            QdrantClient qdrant(target_qdrant_url, target_collection);

            // 0) Qdrant 컬렉션 정보 확인 (디버깅용)
            try {
                json info = qdrant.get_collection_info();
                wcppcli::WLog::info("Qdrant collection info: " + info.dump());
            } catch (const std::exception &e) {
                wcppcli::WLog::warn("Could not fetch Qdrant collection info: " +
                                    std::string(e.what()));
            }

            // 1) 질문 임베딩
            llm_client::EmbeddingParams embed_params;
            embed_params.model = target_embed_model;
            auto embed_res = llm_client->embed({query_}, embed_params);
            if (embed_res.embeddings.empty()) {
                wcppcli::WLog::error("Embedding returned empty result");
                return 1;
            }
            wcppcli::WLog::info("Query embedding dimension: " +
                                std::to_string(embed_res.embeddings[0].size()));

            // 2) Qdrant 검색
            auto hits =
                qdrant.search(embed_res.embeddings[0], target_top_k, target_score_threshold);

            if (hits.empty()) {
                wcppcli::WLog::warn("No relevant documents found in Qdrant");
            } else {
                wcppcli::WLog::info("Retrieved " + std::to_string(hits.size()) +
                                    " chunks from Qdrant");
            }

            // 3) 컨텍스트 조립
            std::string context;
            for (const auto &[text, score] : hits) {
                context += "[score: " + format_score(score) + "] " + text + "\n";
            }

            std::string prompt = "아래 문서를 참고해서 질문에 답하세요. 문서에 관련 정보가 없으면 "
                                 "'제공된 문서에는 답이 없습니다'라고 답하세요.\n\n문서:\n" +
                                 (context.empty() ? "(없음)\n" : context) + "\n질문: " + query_;

            wcppcli::WLog::info("Generated Prompt:\n" + prompt);

            // 4) LLM 생성
            llm_client::RequestParams gen_params;
            gen_params.model = target_model;
            auto response = llm_client->generate(prompt, gen_params);

            std::cout << "\n[Answer]\n" << response.content << std::endl;

        } catch (const std::exception &e) {
            wcppcli::WLog::error("RAG failed: " + std::string(e.what()));
            return 1;
        }
        return 0;
    }

    // CLI > env > fallback 우선순위로 설정값을 선택한다.
    struct ConfigSource {
        const std::string *cli = nullptr;
        std::string env;
        std::string fallback;
    };

    static auto pick_first(const ConfigSource &src) -> std::string {
        if (src.cli != nullptr && !src.cli->empty()) {
            return *src.cli;
        }
        if (!src.env.empty()) {
            return src.env;
        }
        return src.fallback;
    }

    static auto format_score(double score) -> std::string {
        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(4);
        oss << score;
        return oss.str();
    }

    static auto parse_score_threshold(const std::string &value) -> double {
        if (value.empty()) {
            return 0.0;
        }
        try {
            size_t pos = 0;
            double parsed = std::stod(value, &pos);
            if (pos != value.size()) {
                throw std::invalid_argument("trailing characters");
            }
            if (parsed < 0.0 || parsed > 1.0) {
                throw std::out_of_range("score-threshold must be between 0.0 and 1.0");
            }
            return parsed;
        } catch (const std::exception &e) {
            throw std::invalid_argument(std::string("Invalid --score-threshold value: '") + value +
                                         "' (" + e.what() + "). Use a number between 0.0 and 1.0.");
        }
    }

    std::string query_;
    std::string add_text_;
    std::string title_;
    std::string file_path_;
    std::string model_;
    std::string embed_model_;
    std::string llm_url_;
    std::string qdrant_url_;
    std::string collection_;
    std::string score_threshold_str_;
    int top_k_ = 0;
};

} // namespace ragcli::cmd
