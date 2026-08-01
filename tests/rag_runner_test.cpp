#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "mock_qdrant_port.hpp"
#include "rag/rag_config.hpp"
#include "rag/rag_runner.hpp"
#include "test_helper.hpp"

#include "rag_runner_test.hpp"

using ragcli::test::MockLlmPort;
using ragcli::test::MockQdrantPort;

TEST(RagRunner, AddKnowledgeForwardsToQdrant) {
    auto llm_port = std::make_shared<MockLlmPort>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();

    ragcli::rag::RagRunner runner(llm_port, qdrant_port);

    ragcli::rag::RagTargets targets;
    targets.embed_model = "mock-embed-model";
    targets.collection = "test-collection";

    ragcli::rag::RagRunner::AddInput input;
    input.text = "hello world";
    input.title = "greeting";

    const int exit_code = runner.add_knowledge(input, targets);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(qdrant_port->last_upsert_content_, "hello world");
    EXPECT_EQ(qdrant_port->last_upsert_title_, "greeting");
}

TEST(RagRunner, QueryRagBuildsPromptAndGeneratesAnswer) {
    auto llm_port = std::make_shared<MockLlmPort>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();

    ragcli::rag::RagRunner runner(llm_port, qdrant_port);

    ragcli::rag::RagTargets targets;
    targets.model = "mock-model";
    targets.embed_model = "mock-embed-model";

    ragcli::rag::RagRunner::QueryInput input;
    input.query = "what is up?";
    input.top_k = 2;

    ragcli::test::CoutCapture cap;
    const int exit_code = runner.query_rag(input, targets);
    const std::string output = cap.str();

    EXPECT_EQ(exit_code, 0);
    EXPECT_NE(output.find("[Answer]"), std::string::npos);
    EXPECT_NE(output.find("mock answer"), std::string::npos);
}

TEST(RagRunner, EmptyEmbeddingReturnsErrorForAdd) {
    auto llm_port = std::make_shared<MockLlmPort>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();

    llm_port->set_embeddings_returned(false);

    ragcli::rag::RagRunner runner(llm_port, qdrant_port);

    ragcli::rag::RagTargets targets;
    targets.embed_model = "mock-embed-model";

    ragcli::rag::RagRunner::AddInput input;
    input.text = "content";

    const int exit_code = runner.add_knowledge(input, targets);

    EXPECT_EQ(exit_code, 1);
    EXPECT_TRUE(qdrant_port->last_upsert_content_.empty());
}

TEST(RagRunner, QueryRagAttachesRetrievedImageToVisionPrompt) {
    auto llm_port = std::make_shared<MockLlmPort>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    ragcli::rag::SearchHit image_hit;
    image_hit.text = "a relevant chunk";
    image_hit.score = 0.9;
    image_hit.is_image = true;
    image_hit.image_base64 = "base64-image-data";
    qdrant_port->search_results_ = {image_hit};

    ragcli::rag::RagRunner runner(llm_port, qdrant_port);

    ragcli::rag::RagTargets targets;
    targets.model = "mock-model";
    targets.embed_model = "mock-embed-model";

    ragcli::rag::RagRunner::QueryInput input;
    input.query = "what does the chart show?";
    input.top_k = 1;

    ragcli::test::CoutCapture cap;
    const int exit_code = runner.query_rag(input, targets);
    const std::string output = cap.str();

    EXPECT_EQ(exit_code, 0);
    // 이미지가 첨부된 hit 이 있으면 chat()(Vision) 경로를 타야 한다.
    EXPECT_NE(output.find("mock vision answer"), std::string::npos);
}

TEST(RagRunner, QueryRagUsesTextOnlyPromptWhenNoImageHits) {
    auto llm_port = std::make_shared<MockLlmPort>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    ragcli::rag::SearchHit text_hit;
    text_hit.text = "a relevant chunk";
    text_hit.score = 0.9;
    qdrant_port->search_results_ = {text_hit};

    ragcli::rag::RagRunner runner(llm_port, qdrant_port);

    ragcli::rag::RagTargets targets;
    targets.model = "mock-model";
    targets.embed_model = "mock-embed-model";

    ragcli::rag::RagRunner::QueryInput input;
    input.query = "what is up?";
    input.top_k = 1;

    ragcli::test::CoutCapture cap;
    const int exit_code = runner.query_rag(input, targets);
    const std::string output = cap.str();

    EXPECT_EQ(exit_code, 0);
    // 이미지 hit 이 없으면 generate()(텍스트 전용) 경로를 타야 한다.
    EXPECT_NE(output.find("mock answer"), std::string::npos);
    EXPECT_EQ(output.find("mock vision answer"), std::string::npos);
}

TEST(RagRunner, QueryRagWidensSearchLimitForDefaultLexicalRerank) {
    auto llm_port = std::make_shared<MockLlmPort>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    ragcli::rag::SearchHit hit;
    hit.text = "hello";
    hit.score = 0.9;
    qdrant_port->search_results_ = {hit};

    ragcli::rag::RagRunner runner(llm_port, qdrant_port);

    ragcli::rag::RagTargets targets;
    targets.model = "mock-model";
    targets.embed_model = "mock-embed-model";

    ragcli::rag::RagRunner::QueryInput input;
    input.query = "hello";
    input.top_k = 3; // rerank_mode 기본값 "lexical" -> max(3*4, 20) = 20

    ragcli::test::CoutCapture cap;
    const int exit_code = runner.query_rag(input, targets);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(qdrant_port->last_search_limit_, 20);
}

TEST(RagRunner, QueryRagUsesTopKDirectlyWhenRerankDisabled) {
    auto llm_port = std::make_shared<MockLlmPort>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    ragcli::rag::SearchHit hit;
    hit.text = "hello";
    hit.score = 0.9;
    qdrant_port->search_results_ = {hit};

    ragcli::rag::RagRunner runner(llm_port, qdrant_port);

    ragcli::rag::RagTargets targets;
    targets.model = "mock-model";
    targets.embed_model = "mock-embed-model";

    ragcli::rag::RagRunner::QueryInput input;
    input.query = "hello";
    input.top_k = 3;
    input.rerank_mode = "none";

    ragcli::test::CoutCapture cap;
    const int exit_code = runner.query_rag(input, targets);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(qdrant_port->last_search_limit_, 3);
}

TEST(RagRunner, QueryRagRespectsExplicitRerankCandidates) {
    auto llm_port = std::make_shared<MockLlmPort>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();
    ragcli::rag::SearchHit hit;
    hit.text = "hello";
    hit.score = 0.9;
    qdrant_port->search_results_ = {hit};

    ragcli::rag::RagRunner runner(llm_port, qdrant_port);

    ragcli::rag::RagTargets targets;
    targets.model = "mock-model";
    targets.embed_model = "mock-embed-model";

    ragcli::rag::RagRunner::QueryInput input;
    input.query = "hello";
    input.top_k = 3;
    input.rerank_candidates = 50;

    ragcli::test::CoutCapture cap;
    const int exit_code = runner.query_rag(input, targets);

    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(qdrant_port->last_search_limit_, 50);
}

TEST(RagRunner, ParseScoreThresholdAcceptsValidRange) {
    EXPECT_DOUBLE_EQ(ragcli::rag::RagRunner::parse_score_threshold(""), 0.0);
    EXPECT_DOUBLE_EQ(ragcli::rag::RagRunner::parse_score_threshold("0.0"), 0.0);
    EXPECT_DOUBLE_EQ(ragcli::rag::RagRunner::parse_score_threshold("0.5"), 0.5);
    EXPECT_DOUBLE_EQ(ragcli::rag::RagRunner::parse_score_threshold("1.0"), 1.0);
}

TEST(RagRunner, ParseScoreThresholdRejectsOutOfRangeValues) {
    EXPECT_THROW(ragcli::rag::RagRunner::parse_score_threshold("1.5"), std::invalid_argument);
    EXPECT_THROW(ragcli::rag::RagRunner::parse_score_threshold("-0.1"), std::invalid_argument);
}

TEST(RagRunner, ParseScoreThresholdRejectsNonNumericValues) {
    EXPECT_THROW(ragcli::rag::RagRunner::parse_score_threshold("abc"), std::invalid_argument);
    EXPECT_THROW(ragcli::rag::RagRunner::parse_score_threshold("0.5abc"), std::invalid_argument);
}

TEST(RagRunner, EmptyEmbeddingReturnsErrorForQuery) {
    auto llm_port = std::make_shared<MockLlmPort>();
    auto qdrant_port = std::make_shared<MockQdrantPort>();

    llm_port->set_embeddings_returned(false);

    ragcli::rag::RagRunner runner(llm_port, qdrant_port);

    ragcli::rag::RagTargets targets;
    targets.embed_model = "mock-embed-model";

    ragcli::rag::RagRunner::QueryInput input;
    input.query = "question?";

    const int exit_code = runner.query_rag(input, targets);

    EXPECT_EQ(exit_code, 1);
}
