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
