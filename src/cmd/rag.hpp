#pragma once

#include <memory>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wlog.hpp>

#include "chunking/markdown_chunker.hpp"
#include "chunking/no_chunker.hpp"
#include "chunking/simple_chunker.hpp"
#include "command.hpp"
#include "document/document_source_factory.hpp"
#include "embedding/llm_embedding_provider.hpp"
#include "flag_helper.hpp"
#include "indexing/index_options.hpp"
#include "indexing/indexer.hpp"
#include "llm_client/llm_client_factory.hpp"
#include "qdrant/qdrant_client.hpp"
#include "rag/llm_port.hpp"
#include "rag/qdrant_port.hpp"
#include "rag/rag_config.hpp"
#include "rag/rag_runner.hpp"
#include "wcppcli/wconf.hpp"
#include "utils/config_path.hpp"

namespace ragcli::cmd {

// `ragcli rag` 서브커맨드.
// 질문을 임베딩한 뒤 Qdrant 에서 유사 문서를 검색하고, 검색 결과를 컨텍스트로
// LLM 에게 전달해 답변을 생성하거나 지식을 추가한다.
class RagCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "rag";
        cmd->description = "Run retrieval-augmented generation or add knowledge via Qdrant";

        add_string_flag(*cmd, "query", 'q', "User query for RAG search", &query_);
        add_string_flag(*cmd, "add", 'a', "Knowledge text content to add to Qdrant", &add_text_);
        add_string_flag(*cmd, "title", 0, "Title of the knowledge chunk to add", &title_);
        add_string_flag(*cmd, "file", 'f', "File path containing knowledge text to add",
                        &file_path_);
        add_string_flag(*cmd, "pdf", 0, "PDF file path to index into Qdrant", &pdf_path_);
        add_string_flag(*cmd, "dir", 0, "Directory path to recursively index into Qdrant",
                        &dir_path_);
        add_string_flag(*cmd, "image", 0, "Image file path to index into Qdrant", &image_path_);
        add_string_flag(*cmd, "provider", 0,
                        "LLM provider: 'ollama' (default), 'openai', or 'azure'", &provider_);
        add_string_flag(*cmd, "model", 'm', "LLM model name (default depends on --provider)",
                        &model_);
        add_string_flag(*cmd, "embed-model", 'e',
                        "Embedding model name (default depends on --provider)", &embed_model_);
        add_string_flag(*cmd, "llm-url", 0,
                        "LLM base URL (default depends on --provider, e.g. "
                        "http://localhost:11434 for ollama)",
                        &llm_url_);
        add_string_flag(*cmd, "qdrant-url", 0, "Qdrant base URL (default: http://localhost:6333)",
                        &qdrant_url_);
        add_string_flag(*cmd, "collection", 'c', "Qdrant collection name (default: documents)",
                        &collection_);
        add_int_flag(*cmd, "top-k", 'k', "Number of retrieved chunks (default: 3)", &top_k_);
        add_string_flag(*cmd, "score-threshold", 0,
                        "Minimum Qdrant search score 0.0~1.0 (default: 0.0 = disabled)",
                        &score_threshold_str_);
        add_string_flag(*cmd, "chunker", 0,
                        "Chunking strategy: 'simple' or 'markdown' (default: simple)",
                        &chunker_type_);
        add_int_flag(*cmd, "chunk-size", 0,
                     "Simple chunker: chunk size in characters (default: 512)", &chunk_size_);
        add_int_flag(*cmd, "chunk-overlap", 0,
                     "Simple chunker: overlap between chunks in characters (default: 64)",
                     &chunk_overlap_);
        add_string_flag(*cmd, "distance", 0,
                        "Qdrant distance metric: 'Cosine' (default), 'Euclid', 'Dot', or "
                        "'Manhattan'",
                        &distance_);

        cmd->handler = [this](const wcppcli::Command & /*unused*/) { return run_rag(); };

        root.add_command(std::move(cmd));
    }

  private:
    auto run_rag() -> int {
        wcppcli::WConf conf;
        ragcli::utils::load_config(conf);

        // 설정 우선순위: CLI 플래그 > 환경 변수 > 코드 기본값
        rag::RagCliOverrides overrides{llm_url_,    model_,      embed_model_,
                                       qdrant_url_, collection_, provider_};
        rag::RagTargets targets = rag::RagRunner::resolve_targets(overrides, conf);

        auto llm_client = llm_client::LLMClientFactory::create(
            targets.provider, targets.api_key, targets.llm_url, targets.api_version);
        auto llm_port = std::make_shared<rag::LlmClientAdapter>(std::move(llm_client));

        auto qdrant_client =
            std::make_shared<ragcli::qdrant::QdrantClient>(targets.qdrant_url, targets.collection);
        auto qdrant_port = std::make_shared<rag::QdrantClientAdapter>(qdrant_client);

        std::shared_ptr<chunking::Chunker> chunker;
        if (chunker_type_ == "markdown") {
            chunker = std::make_shared<chunking::MarkdownChunker>();
        } else {
            const int chunk_size =
                rag::pick_first_positive_int(chunk_size_, conf.get_int("CHUNK_SIZE"),
                                             static_cast<int>(chunking::k_default_chunk_size));
            const int chunk_overlap =
                rag::pick_first_positive_int(chunk_overlap_, conf.get_int("CHUNK_OVERLAP"),
                                             static_cast<int>(chunking::k_default_overlap));
            chunker = std::make_shared<chunking::SimpleChunker>(
                chunking::ChunkSize{static_cast<std::size_t>(chunk_size)},
                chunking::Overlap{static_cast<std::size_t>(chunk_overlap)});
        }

        const std::string distance =
            rag::pick_first({&distance_, conf.get_string("QDRANT_DISTANCE"), "Cosine"});

        // 1) 인덱싱 모드: --pdf, --dir, --image
        if (!pdf_path_.empty()) {
            return run_index(document::create_source_from_path(pdf_path_), targets, llm_port,
                             qdrant_port, chunker, distance);
        }
        if (!dir_path_.empty()) {
            return run_index(document::create_source_from_path(dir_path_), targets, llm_port,
                             qdrant_port, chunker, distance);
        }
        if (!image_path_.empty()) {
            return run_index(document::create_source_from_path(image_path_), targets, llm_port,
                             qdrant_port, std::make_shared<chunking::NoChunker>(), distance);
        }

        rag::RagRunner runner(llm_port, qdrant_port);

        // 2) 기존 단일 텍스트 지식 추가 모드 (-a / --add 또는 -f / --file)
        if (!add_text_.empty() || !file_path_.empty()) {
            return runner.add_knowledge({add_text_, file_path_, title_}, targets);
        }

        // 3) 질문 모드 처리 (-q / --query)
        if (!query_.empty()) {
            return runner.query_rag({query_, top_k_, score_threshold_str_}, targets);
        }

        wcppcli::WLog::error(
            "Either --query (-q) or one of --add (-a) / --file (-f) / --pdf / --dir / --image "
            "must be specified.");
        return 1;
    }

    static auto run_index(const std::shared_ptr<document::DocumentSource> &source,
                          const rag::RagTargets &targets, std::shared_ptr<rag::LlmPort> llm_port,
                          std::shared_ptr<rag::QdrantPort> qdrant_port,
                          std::shared_ptr<chunking::Chunker> chunker,
                          const std::string &distance) -> int {
        auto embed_provider =
            std::make_shared<embedding::LlmEmbeddingProvider>(std::move(llm_port));

        indexing::IndexOptions options;
        options.embed_model = targets.embed_model;
        options.source_type = "ragcli_index";
        options.distance = distance;

        indexing::Indexer indexer(embed_provider, std::move(qdrant_port), std::move(chunker));
        return indexer.index(source, options);
    }

    std::string query_;
    std::string add_text_;
    std::string title_;
    std::string file_path_;
    std::string pdf_path_;
    std::string dir_path_;
    std::string image_path_;
    std::string model_;
    std::string embed_model_;
    std::string llm_url_;
    std::string qdrant_url_;
    std::string collection_;
    std::string provider_;
    std::string score_threshold_str_;
    std::string chunker_type_;
    std::string distance_;
    int top_k_ = 0;
    int chunk_size_ = 0;
    int chunk_overlap_ = 0;
};

} // namespace ragcli::cmd
