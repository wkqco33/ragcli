#pragma once

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <wcppcli/wconf.hpp>
#include <wcppcli/wlog.hpp>

#include "rag/llm_port.hpp"
#include "rag/prompt_builder.hpp"
#include "rag/qdrant_port.hpp"
#include "rag/rag_config.hpp"

namespace ragcli::rag {

// RagCommand 에서 전달하는 CLI 플래그 값들.
struct RagCliOverrides {
    std::string llm_url;
    std::string model;
    std::string embed_model;
    std::string qdrant_url;
    std::string collection;
};

// RAG 지식 추가/질의 실행기.
// LLM 과 Qdrant 는 생성자로 주입받아 테스트 시 Mock 으로 대체 가능하다.
class RagRunner {
  public:
    RagRunner(std::shared_ptr<LlmPort> llm_port, std::shared_ptr<QdrantPort> qdrant_port)
        : llm_port_(std::move(llm_port)), qdrant_port_(std::move(qdrant_port)) {}

    struct AddInput {
        std::string text;      // --add 로 직접 입력한 텍스트
        std::string file_path; // --file 로 지정한 파일 경로
        std::string title;     // --title 로 지정한 제목
    };

    struct QueryInput {
        std::string query;
        int top_k = 0;
        std::string score_threshold_str;
    };

    // CLI 플래그 > 환경 변수 > 코드 기본값 순으로 최종 설정을 결정한다.
    static auto resolve_targets(const RagCliOverrides &overrides, const wcppcli::WConf &conf)
        -> RagTargets {
        RagTargets targets;
        targets.llm_url = pick_first(
            {&overrides.llm_url, conf.get_string("OLLAMA_BASE_URL"), "http://localhost:11434"});
        targets.model = pick_first({&overrides.model, conf.get_string("OLLAMA_MODEL"), "llama3"});
        targets.embed_model = pick_first(
            {&overrides.embed_model, conf.get_string("OLLAMA_EMBED_MODEL"), "nomic-embed-text"});
        targets.qdrant_url = pick_first(
            {&overrides.qdrant_url, conf.get_string("QDRANT_BASE_URL"), "http://localhost:6333"});
        targets.collection =
            pick_first({&overrides.collection, conf.get_string("QDRANT_COLLECTION"), "documents"});
        return targets;
    }

    // score-threshold 문자열을 0.0~1.0 범위의 double 로 파싱한다.
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

    // 지식 추가 모드를 실행한다.
    auto add_knowledge(const AddInput &input, const RagTargets &targets) const -> int {
        std::string content_to_add = input.text;

        if (!input.file_path.empty()) {
            content_to_add = read_file_content(input.file_path);
        }

        if (content_to_add.empty()) {
            wcppcli::WLog::error("Content to add is empty.");
            return 1;
        }

        try {
            wcppcli::WLog::info("Embedding knowledge text (" +
                                std::to_string(content_to_add.size()) + " bytes)...");

            llm_client::EmbeddingParams embed_params;
            embed_params.model = targets.embed_model;
            auto embed_res = llm_port_->embed({content_to_add}, embed_params);

            if (embed_res.embeddings.empty()) {
                wcppcli::WLog::error("Embedding returned empty result.");
                return 1;
            }

            qdrant_port_->upsert_point(embed_res.embeddings[0], {content_to_add, input.title, "",
                                                                 "ragcli_add", 0, 0, false, 0, 0, ""});
            wcppcli::WLog::success("Successfully added knowledge to Qdrant collection '" +
                                   targets.collection + "'.");
        } catch (const std::exception &e) {
            wcppcli::WLog::error("Add knowledge failed: " + std::string(e.what()));
            return 1;
        }

        return 0;
    }

    // RAG 질의 모드를 실행한다.
    auto query_rag(const QueryInput &input, const RagTargets &targets) const -> int {
        int target_top_k = input.top_k > 0 ? input.top_k : 3;
        double target_score_threshold = parse_score_threshold(input.score_threshold_str);

        try {
            // 1) 질문 임베딩
            llm_client::EmbeddingParams embed_params;
            embed_params.model = targets.embed_model;
            auto embed_res = llm_port_->embed({input.query}, embed_params);
            if (embed_res.embeddings.empty()) {
                wcppcli::WLog::error("Embedding returned empty result");
                return 1;
            }
            wcppcli::WLog::info("Query embedding dimension: " +
                                std::to_string(embed_res.embeddings[0].size()));

            // 2) Qdrant 검색
            auto hits =
                qdrant_port_->search(embed_res.embeddings[0], target_top_k, target_score_threshold);

            if (hits.empty()) {
                wcppcli::WLog::warn("No relevant documents found in Qdrant");
            } else {
                wcppcli::WLog::info("Retrieved " + std::to_string(hits.size()) +
                                    " chunks from Qdrant");
            }

            // 3) 프롬프트 생성 및 LLM 호출 (멀티모달 Vision 연동)
            std::string prompt = build_query_prompt(hits, input.query);
            wcppcli::WLog::info("Generated Prompt:\n" + prompt);

            llm_client::RequestParams gen_params;
            gen_params.model = targets.model;

            llm_client::ResponseData response;
            std::vector<llm_client::ContentBlock> blocks;
            blocks.push_back(llm_client::ContentBlock::makeText(prompt));

            for (const auto &hit : hits) {
                if (hit.is_image && !hit.image_base64.empty()) {
                    wcppcli::WLog::info("Attaching retrieved image to Vision LLM prompt.");
                    blocks.push_back(llm_client::ContentBlock::makeImageBase64(hit.image_base64));
                }
            }

            if (blocks.size() > 1) {
                llm_client::Message msg("user", blocks);
                response = llm_port_->chat({msg}, gen_params);
            } else {
                response = llm_port_->generate(prompt, gen_params);
            }

            std::cout << "\n[Answer]\n" << response.content << std::endl;

        } catch (const std::exception &e) {
            wcppcli::WLog::error("RAG failed: " + std::string(e.what()));
            return 1;
        }
        return 0;
    }

  private:
    std::shared_ptr<LlmPort> llm_port_;
    std::shared_ptr<QdrantPort> qdrant_port_;

    static auto read_file_content(const std::string &file_path) -> std::string {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + file_path);
        }
        std::ostringstream buffer_stream;
        buffer_stream << file.rdbuf();
        return buffer_stream.str();
    }
};

} // namespace ragcli::rag
