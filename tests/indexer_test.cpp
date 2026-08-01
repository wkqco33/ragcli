#include <array>
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

namespace {
constexpr std::size_t k_long_text_length = 1000;
constexpr ragcli::chunking::ChunkSize k_test_chunk_size{256};
constexpr ragcli::chunking::Overlap k_test_overlap{32};

constexpr int k_ppm_width = 2;
constexpr int k_ppm_height = 2;
constexpr int k_ppm_channels = 3;
constexpr int k_ppm_max_value = 255;
constexpr std::size_t k_ppm_pixel_bytes = static_cast<std::size_t>(k_ppm_width) *
                                          static_cast<std::size_t>(k_ppm_height) *
                                          static_cast<std::size_t>(k_ppm_channels);
} // namespace

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

TEST(Indexer, PropagatesFileSourcePathAndChunkZeroIndexFromDirectoryIndexing) {
    ragcli::document::ExtractedPage page;
    page.text = "short content that stays as a single chunk";
    page.title = "file.txt";
    page.source_path = "kb/file.txt"; // 디렉터리 인덱싱 시 실제 파일 경로 (source_name 은 디렉터리)
    std::vector<ragcli::document::ExtractedPage> pages{page};

    auto source = std::make_shared<MockDocumentSource>(pages, "kb");
    auto embed_provider = std::make_shared<MockEmbeddingProvider>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    auto chunker = std::make_shared<ragcli::chunking::NoChunker>();

    ragcli::indexing::Indexer indexer(embed_provider, qdrant_port, chunker);

    ragcli::indexing::IndexOptions options;
    options.embed_model = "mock-model";

    const int exit_code = indexer.index(source, options);

    EXPECT_EQ(exit_code, 0);
    ASSERT_EQ(qdrant_port->last_data_.size(), 1U);
    EXPECT_EQ(qdrant_port->last_data_[0].chunk_index, 0);   // 0번 청크도 순서 정보가 전달되어야 함
    EXPECT_EQ(qdrant_port->last_data_[0].source, "kb/file.txt"); // 디렉터리가 아니라 파일 경로
    EXPECT_EQ(qdrant_port->last_data_[0].chunk_total, 1);
}

TEST(Indexer, SimpleChunkerSplitsLongText) {
    std::vector<ragcli::document::ExtractedPage> pages;
    pages.push_back({std::string(k_long_text_length, 'a'), "long.txt", 0, {}, 0, 0});

    auto source = std::make_shared<MockDocumentSource>(pages, "long.txt");
    auto embed_provider = std::make_shared<MockEmbeddingProvider>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    auto chunker =
        std::make_shared<ragcli::chunking::SimpleChunker>(k_test_chunk_size, k_test_overlap);

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
        ppm << "P6\n" << k_ppm_width << ' ' << k_ppm_height << "\n" << k_ppm_max_value << "\n";
        std::array<unsigned char, k_ppm_pixel_bytes> pixels = {k_ppm_max_value,
                                                               0,
                                                               0, // red
                                                               0,
                                                               k_ppm_max_value,
                                                               0, // green
                                                               0,
                                                               0,
                                                               k_ppm_max_value, // blue
                                                               k_ppm_max_value,
                                                               k_ppm_max_value,
                                                               k_ppm_max_value}; // white
        ppm.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
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

TEST(NoChunker, PassesThroughOnePageToOneChunkWithImageData) {
    std::vector<ragcli::document::ExtractedPage> pages;
    ragcli::document::ExtractedPage text_page;
    text_page.text = "plain text page";
    text_page.title = "note.txt";
    pages.push_back(text_page);

    ragcli::document::ExtractedPage image_page;
    image_page.title = "photo";
    image_page.is_image = true;
    image_page.image_width = 2;
    image_page.image_height = 2;
    image_page.image = {1, 2, 3, 4};
    pages.push_back(image_page);

    ragcli::chunking::NoChunker chunker;
    auto chunks = chunker.chunk(pages, "source.txt");

    ASSERT_EQ(chunks.size(), 2U);

    EXPECT_EQ(chunks[0].text, "plain text page");
    EXPECT_EQ(chunks[0].title, "note.txt");
    EXPECT_EQ(chunks[0].source, "source.txt");
    EXPECT_EQ(chunks[0].chunk_index, 0);
    EXPECT_FALSE(chunks[0].is_image);

    EXPECT_TRUE(chunks[1].is_image);
    EXPECT_EQ(chunks[1].chunk_index, 1);
    EXPECT_EQ(chunks[1].image_width, 2);
    EXPECT_EQ(chunks[1].image_height, 2);
    EXPECT_FALSE(chunks[1].image_base64.empty()); // 원본 이미지가 있으면 base64 로 인코딩되어야 함
}

TEST(Base64, EncodesBinaryData) {
    std::vector<uint8_t> data = {'A', 'B', 'C'};
    std::string encoded = ragcli::utils::base64_encode(data);
    EXPECT_EQ(encoded, "QUJD");
}
