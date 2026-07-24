#include <string>

#include <gtest/gtest.h>
#include <wcppcli/wconf.hpp>

#include "llm/provider_config.hpp"

using ragcli::llm::ProviderOverrides;
using ragcli::llm::resolve_provider_targets;

TEST(ProviderConfig, DefaultsToOllamaWhenNothingSet) {
    wcppcli::WConf conf;

    auto targets = resolve_provider_targets({}, conf);

    EXPECT_EQ(targets.provider, "ollama");
    EXPECT_EQ(targets.base_url, "http://localhost:11434");
    EXPECT_EQ(targets.model, "llama3");
    EXPECT_EQ(targets.embed_model, "nomic-embed-text");
    EXPECT_TRUE(targets.api_key.empty());
}

TEST(ProviderConfig, OpenAiUsesApiKeyAndDefaultModel) {
    wcppcli::WConf conf;
    conf.set("LLM_PROVIDER", std::string("openai"));
    conf.set("OPENAI_API_KEY", std::string("sk-test"));

    auto targets = resolve_provider_targets({}, conf);

    EXPECT_EQ(targets.provider, "openai");
    EXPECT_EQ(targets.model, "gpt-4o-mini");
    EXPECT_EQ(targets.embed_model, "text-embedding-3-small");
    EXPECT_EQ(targets.api_key, "sk-test");
    EXPECT_TRUE(targets.base_url.empty()); // openai client falls back to its own default endpoint
}

TEST(ProviderConfig, AzureRequiresExplicitModelAndReadsApiVersion) {
    wcppcli::WConf conf;
    conf.set("LLM_PROVIDER", std::string("azure"));
    conf.set("AZURE_OPENAI_API_KEY", std::string("azure-key"));
    conf.set("AZURE_OPENAI_BASE_URL", std::string("https://example.openai.azure.com/"));
    conf.set("AZURE_OPENAI_MODEL", std::string("my-deployment"));
    conf.set("AZURE_OPENAI_API_VERSION", std::string("2024-05-01"));

    auto targets = resolve_provider_targets({}, conf);

    EXPECT_EQ(targets.provider, "azure");
    EXPECT_EQ(targets.api_key, "azure-key");
    EXPECT_EQ(targets.base_url, "https://example.openai.azure.com/");
    EXPECT_EQ(targets.model, "my-deployment");
    EXPECT_EQ(targets.api_version, "2024-05-01");
}

TEST(ProviderConfig, CliOverrideWinsOverEnvAndDefault) {
    wcppcli::WConf conf;
    conf.set("LLM_PROVIDER", std::string("openai"));
    conf.set("OPENAI_MODEL", std::string("from-env"));

    const std::string cli_provider = "ollama";
    const std::string cli_model = "from-cli";
    ProviderOverrides overrides{&cli_provider, nullptr, &cli_model, nullptr};

    auto targets = resolve_provider_targets(overrides, conf);

    // provider 자체는 CLI가 이겨서 ollama 로 확정되고, 그에 따라 model 도
    // ollama 브랜치에서 CLI 오버라이드를 적용한다.
    EXPECT_EQ(targets.provider, "ollama");
    EXPECT_EQ(targets.model, "from-cli");
}

TEST(ProviderConfig, UnknownProviderPassesThroughForLlmClientToReject) {
    wcppcli::WConf conf;
    conf.set("LLM_PROVIDER", std::string("anthropic"));

    auto targets = resolve_provider_targets({}, conf);

    // anthropic/gemini 는 llm_client 라이브러리 자체가 지원하지 않으므로
    // 여기서는 값만 그대로 전달하고, 실제 거부는 LLMClientFactory::create 가 담당한다.
    EXPECT_EQ(targets.provider, "anthropic");
}
