#include <fstream>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "chunking/markdown_chunker.hpp"
#include "chunking/no_chunker.hpp"
#include "chunking/simple_chunker.hpp"
#include "document/document_source.hpp"
#include "document/image_source.hpp"
#include "indexing/index_options.hpp"
#include "indexing/indexer.hpp"
#include "mock_qdrant_port.hpp"
#include "utils/base64.hpp"

#include "indexer_test.hpp"

using ragcli::test::MockDocumentSource;
using ragcli::test::MockEmbeddingProvider;
using ragcli::test::MockQdrantPort;

TEST(Indexer, IndexesTextChunksToQdrant) {
    std::vector<ragcli::document::ExtractedPage> pages;
    pages.push_back({"hello world this is a test document", "test.txt", 0, {}, 0, 0});

    auto source = std::make_shared<MockDocumentSource>(pages, "test.txt");
    auto embed_provider = std::make_shared<MockEmbeddingProvider>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    auto chunker = std::make_shared<ragcli::chunking::NoChunker>();

    ragcli::indexing::Indexer indexer(embed_provider, qdrant_port, chunker);

    ragcli::indexing::IndexOptions options;
    options.embed_model = "mock-model";

    const int exit_code = indexer.index(source, options);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(qdrant_port->last_data_.size(), 1U);
    EXPECT_EQ(qdrant_port->last_upsert_content_, "hello world this is a test document");
    EXPECT_EQ(qdrant_port->last_upsert_title_, "test.txt");
}

TEST(Indexer, EmptyContentReturnsZeroWithoutUpsert) {
    auto source = std::make_shared<MockDocumentSource>(
        std::vector<ragcli::document::ExtractedPage>{}, "empty.txt");
    auto embed_provider = std::make_shared<MockEmbeddingProvider>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    auto chunker = std::make_shared<ragcli::chunking::NoChunker>();

    ragcli::indexing::Indexer indexer(embed_provider, qdrant_port, chunker);

    ragcli::indexing::IndexOptions options;
    options.embed_model = "mock-model";

    const int exit_code = indexer.index(source, options);

    EXPECT_EQ(exit_code, 0);
    EXPECT_TRUE(qdrant_port->last_data_.empty());
}

TEST(Indexer, EmptyEmbeddingReturnsError) {
    class EmptyEmbeddingProvider : public ragcli::embedding::EmbeddingProvider {
      public:
        auto embed(const std::vector<std::string> & /*texts*/, const std::string & /*model*/) const
            -> std::vector<std::vector<float>> override {
            return {};
        }
    };

    std::vector<ragcli::document::ExtractedPage> pages;
    pages.push_back({"content", "test.txt", 0, {}, 0, 0});

    auto source = std::make_shared<MockDocumentSource>(pages, "test.txt");
    auto embed_provider = std::make_shared<EmptyEmbeddingProvider>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    auto chunker = std::make_shared<ragcli::chunking::NoChunker>();

    ragcli::indexing::Indexer indexer(embed_provider, qdrant_port, chunker);

    ragcli::indexing::IndexOptions options;
    options.embed_model = "mock-model";

    const int exit_code = indexer.index(source, options);

    EXPECT_EQ(exit_code, 1);
    EXPECT_TRUE(qdrant_port->last_data_.empty());
}

TEST(Indexer, SimpleChunkerSplitsLongText) {
    std::vector<ragcli::document::ExtractedPage> pages;
    pages.push_back({std::string(1000, 'a'), "long.txt", 0, {}, 0, 0});

    auto source = std::make_shared<MockDocumentSource>(pages, "long.txt");
    auto embed_provider = std::make_shared<MockEmbeddingProvider>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    auto chunker = std::make_shared<ragcli::chunking::SimpleChunker>(256, 32);

    ragcli::indexing::Indexer indexer(embed_provider, qdrant_port, chunker);

    ragcli::indexing::IndexOptions options;
    options.embed_model = "mock-model";

    const int exit_code = indexer.index(source, options);

    EXPECT_EQ(exit_code, 0);
    EXPECT_GT(qdrant_port->last_data_.size(), 1U);
}

TEST(ImageFileSource, DecodesPpmImage) {
    const std::string ppm_path = "/tmp/ragcli_test_image.ppm";
    {
        std::ofstream ppm(ppm_path, std::ios::binary);
        // 2x2 RGB 이미지 (P6 binary PPM)
        ppm << "P6\n2 2\n255\n";
        const unsigned char pixels[] = {255, 0,   0,    // red
                                        0,   255, 0,    // green
                                        0,   0,   255,  // blue
                                        255, 255, 255}; // white
        ppm.write(reinterpret_cast<const char *>(pixels), sizeof(pixels));
    }

    auto source = std::make_shared<ragcli::document::ImageFileSource>(ppm_path);
    auto pages = source->extract();

    ASSERT_EQ(pages.size(), 1U);
    EXPECT_EQ(pages[0].image_width, 2);
    EXPECT_EQ(pages[0].image_height, 2);
    EXPECT_EQ(pages[0].image.size(), 16U); // 2 * 2 * 4
    EXPECT_TRUE(pages[0].is_image);
}

TEST(ImageFileSource, ThrowsOnMissingFile) {
    auto source =
        std::make_shared<ragcli::document::ImageFileSource>("/tmp/ragcli_nonexistent.png");
    EXPECT_THROW(source->extract(), std::runtime_error);
}

TEST(MarkdownChunker, SplitsByHeadings) {
    std::vector<ragcli::document::ExtractedPage> pages;
    pages.push_back({"# Section 1\nContent 1\n## Section 2\nContent 2\n", "doc.md", 0, {}, 0, 0});

    ragcli::chunking::MarkdownChunker chunker;
    auto chunks = chunker.chunk(pages, "doc.md");

    ASSERT_EQ(chunks.size(), 2U);
    EXPECT_EQ(chunks[0].title, "Section 1");
    EXPECT_EQ(chunks[1].title, "Section 2");
}

TEST(Base64, EncodesBinaryData) {
    std::vector<uint8_t> data = {'A', 'B', 'C'};
    std::string encoded = ragcli::utils::base64_encode(data);
    EXPECT_EQ(encoded, "QUJD");
}
