#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <wcppcli/wlog.hpp>

#include <future>

#include "chunking/chunk.hpp"
#include "chunking/chunker_interface.hpp"
#include "document/document_source.hpp"
#include "embedding/embedding_provider.hpp"
#include "indexing/index_options.hpp"
#include "rag/qdrant_port.hpp"

namespace ragcli::indexing {

// DocumentSource -> Chunk -> Embedding -> Qdrant upsert 오케스트레이션.
class Indexer {
  public:
    Indexer(std::shared_ptr<embedding::EmbeddingProvider> embed_provider,
            std::shared_ptr<rag::QdrantPort> qdrant_port,
            std::shared_ptr<chunking::Chunker> chunker)
        : embed_provider_(std::move(embed_provider)), qdrant_port_(std::move(qdrant_port)),
          chunker_(std::move(chunker)) {}

    auto index(std::shared_ptr<const document::DocumentSource> source,
               const IndexOptions &options) const -> int {
        try {
            const std::string source_name = source->source_name();
            wcppcli::WLog::info("Indexing source: " + source_name);

            auto pages = source->extract();
            if (pages.empty()) {
                wcppcli::WLog::warn("No content extracted from: " + source_name);
                return 0;
            }

            auto chunks = chunker_->chunk(pages, source_name);
            if (chunks.empty()) {
                wcppcli::WLog::warn("No chunks produced from: " + source_name);
                return 0;
            }

            const auto embeddings = embed_chunks(chunks, options.embed_model);
            if (embeddings.empty() || embeddings.size() != chunks.size()) {
                wcppcli::WLog::error("Embedding returned invalid count (" + std::to_string(embeddings.size()) +
                                     " embeddings for " + std::to_string(chunks.size()) + " chunks).");
                return 1;
            }

            if (options.auto_create_collection && !embeddings.empty()) {
                const int vector_size = static_cast<int>(embeddings[0].size());
                qdrant_port_->create_collection(vector_size, options.distance);
            }

            for (std::size_t i = 0; i < chunks.size(); ++i) {
                rag::UpsertPoint data;
                data.content = chunks[i].text;
                data.title = build_title(chunks[i]);
                data.source = chunks[i].source;
                data.source_type = options.source_type;
                data.page_index = chunks[i].page_index;
                data.chunk_index = chunks[i].chunk_index;
                data.is_image = chunks[i].is_image;
                data.image_width = chunks[i].image_width;
                data.image_height = chunks[i].image_height;
                data.image_base64 = chunks[i].image_base64;

                qdrant_port_->upsert_point(embeddings[i], data);
            }

            wcppcli::WLog::success("Indexed " + std::to_string(chunks.size()) + " chunks from " +
                                   source_name);

        } catch (const std::exception &e) {
            wcppcli::WLog::error("Indexing failed: " + std::string(e.what()));
            return 1;
        }

        return 0;
    }

  private:
    auto embed_chunks(const std::vector<chunking::Chunk> &chunks, const std::string &model) const
        -> std::vector<std::vector<float>> {
        if (chunks.empty()) {
            return {};
        }

        constexpr std::size_t k_batch_size = 16;
        const std::size_t total_chunks = chunks.size();

        if (total_chunks <= k_batch_size) {
            std::vector<std::string> texts;
            texts.reserve(total_chunks);
            for (const auto &chunk : chunks) {
                texts.push_back(chunk.text);
            }
            return embed_provider_->embed(texts, model);
        }

        std::size_t num_batches = (total_chunks + k_batch_size - 1) / k_batch_size;
        std::vector<std::future<std::vector<std::vector<float>>>> futures;
        futures.reserve(num_batches);

        for (std::size_t b = 0; b < num_batches; ++b) {
            std::size_t start = b * k_batch_size;
            std::size_t end = std::min(start + k_batch_size, total_chunks);

            std::vector<std::string> batch_texts;
            batch_texts.reserve(end - start);
            for (std::size_t i = start; i < end; ++i) {
                batch_texts.push_back(chunks[i].text);
            }

            futures.push_back(std::async(
                std::launch::async,
                [this, texts = std::move(batch_texts), model]() {
                    return embed_provider_->embed(texts, model);
                }));
        }

        std::vector<std::vector<float>> all_embeddings;
        all_embeddings.reserve(total_chunks);

        for (auto &fut : futures) {
            auto batch_res = fut.get();
            for (auto &emb : batch_res) {
                all_embeddings.push_back(std::move(emb));
            }
        }

        return all_embeddings;
    }

    static auto build_title(const chunking::Chunk &chunk) -> std::string {
        if (!chunk.title.empty()) {
            return chunk.title;
        }
        return chunk.source;
    }

    std::shared_ptr<embedding::EmbeddingProvider> embed_provider_;
    std::shared_ptr<rag::QdrantPort> qdrant_port_;
    std::shared_ptr<chunking::Chunker> chunker_;
};

} // namespace ragcli::indexing
