#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wconf.hpp>
#include <wcppcli/wlog.hpp>

#include "chat/chat_config.hpp"
#include "command.hpp"
#include "document/document_source_factory.hpp"
#include "flag_helper.hpp"
#include "llm/provider_config.hpp"
#include "llm_client/llm_client_factory.hpp"
#include "rag/llm_port.hpp"

namespace ragcli::cmd {

// `ragcli pdf` 서브커맨드.
// PDF 파일을 읽어 텍스트를 추출한 뒤 LLM 에게 전달하여 요약 정리를 수행한다.
// Qdrant 나 임베딩 없이 순수하게 LLM 한 번 호출로 요약한다.
class PdfSummarizeCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "pdf";
        cmd->description = "Read a PDF file and summarize its content using an LLM";

        add_string_flag(*cmd, "file", 'f', "PDF file path to summarize", &file_path_);
        add_string_flag(*cmd, "provider", 0,
                        "LLM provider: 'ollama' (default), 'openai', or 'azure'", &provider_);
        add_string_flag(*cmd, "model", 'm', "LLM model name (default depends on --provider)",
                        &model_);
        add_string_flag(*cmd, "url", 'u',
                        "LLM base URL (default depends on --provider, e.g. "
                        "http://localhost:11434 for ollama)",
                        &base_url_);
        add_string_flag(*cmd, "language", 'l',
                        "Summary output language (e.g. 'ko', 'en'). Default: ko", &language_);

        cmd->handler = [this](const wcppcli::Command & /*unused*/) { return run_summarize(); };

        root.add_command(std::move(cmd));
    }

  private:
    auto run_summarize() -> int {
        if (file_path_.empty()) {
            wcppcli::WLog::error("--file (-f) is required. Specify a PDF file path.");
            return 1;
        }

        wcppcli::WConf conf;
        conf.read_file(".env");

        // LLM 프로바이더 설정 해석 (chat 커맨드와 동일한 방식)
        llm::ProviderOverrides overrides{&provider_, &base_url_, &model_, nullptr};
        llm::ProviderTargets targets = llm::resolve_provider_targets(overrides, conf);

        // 1) PDF 텍스트 추출
        wcppcli::WLog::info("Extracting text from PDF: " + file_path_);
        auto source = document::create_source_from_path(file_path_);
        auto pages = source->extract();

        std::string full_text;
        for (const auto &page : pages) {
            if (!page.is_image && !page.text.empty()) {
                if (!full_text.empty()) {
                    full_text += "\n\n";
                }
                full_text += page.text;
            }
        }

        if (full_text.empty()) {
            wcppcli::WLog::error("No text content could be extracted from the PDF.");
            return 1;
        }

        wcppcli::WLog::info("Extracted " + std::to_string(full_text.size()) +
                            " characters. Requesting summary from LLM (" + targets.provider +
                            " / " + targets.model + ")...");

        // 2) 요약 프롬프트 생성
        const std::string lang = language_.empty() ? "ko" : language_;
        std::string prompt = build_summary_prompt(full_text, lang);

        // 3) LLM 호출 (스트리밍)
        auto llm_client = llm_client::LLMClientFactory::create(
            targets.provider, targets.api_key, targets.base_url, targets.api_version);

        llm_client::RequestParams gen_params;
        gen_params.model = targets.model;

        std::cout << "\n[Summary]\n";
        bool any_chunk_streamed = false;
        llm_client::StreamCallback print_chunk = [&any_chunk_streamed](const std::string &chunk) {
            any_chunk_streamed = true;
            std::cout << chunk << std::flush;
        };

        try {
            auto response = llm_client->generateStream(prompt, print_chunk, gen_params);
            if (!any_chunk_streamed) {
                std::cout << response.content;
            }
            std::cout << std::endl;
        } catch (const std::exception &e) {
            wcppcli::WLog::error("LLM summarization failed: " + std::string(e.what()));
            return 1;
        }

        return 0;
    }

    static auto build_summary_prompt(const std::string &text, const std::string &language)
        -> std::string {
        std::string lang_instruction;
        if (language == "ko" || language == "한국어" || language == "korean") {
            lang_instruction =
                "요약은 한국어로 작성하세요. ";
        } else if (language == "en" || language == "english") {
            lang_instruction = "Write the summary in English. ";
        } else {
            lang_instruction = "Write the summary in " + language + ". ";
        }

        return std::string(
                   "아래 PDF 문서의 텍스트를 읽고 핵심 내용을 요약해주세요. ") +
               lang_instruction +
               "문서의 주요 주제, 핵심 포인트, 그리고 중요한 세부 사항을 포함하여 "
               "명확하고 간결하게 정리해주세요.\n\n"
               "--- 문서 내용 ---\n" +
               text +
               "\n--- 문서 내용 끝 ---\n\n"
               "위 문서에 대한 요약을 작성해주세요.";
    }

    std::string file_path_;
    std::string model_;
    std::string base_url_;
    std::string provider_;
    std::string language_;
};

} // namespace ragcli::cmd