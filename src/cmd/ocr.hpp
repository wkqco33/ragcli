#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <wcppcli/wcli.hpp>
#include <wcppcli/wconf.hpp>
#include <wcppcli/wlog.hpp>

#include "command.hpp"
#include "flag_helper.hpp"
#include "llm/provider_config.hpp"
#include "llm_client/llm_client_factory.hpp"
#include "utils/base64.hpp"
#include "utils/config_path.hpp"

namespace ragcli::cmd {

// `ragcli ocr` 서브커맨드.
// 영수증, 문서 스캔 등 이미지 파일의 텍스트를 Vision LLM 으로 추출(OCR)하고
// 핵심 내용을 요약 정리한다. Qdrant 나 임베딩 없이 LLM 한 번 호출로 동작한다.
class OcrCommand : public CommandBase {
  public:
    void register_to(wcppcli::Command &root) override {
        auto cmd = std::make_unique<wcppcli::Command>();
        cmd->name = "ocr";
        cmd->description = "Extract text from an image (OCR) and summarize using a Vision LLM";

        add_string_flag(*cmd, "file", 'f', "Image file path to OCR (receipt, scan, etc.)",
                        &file_path_);
        add_string_flag(*cmd, "provider", 0,
                        "LLM provider: 'ollama' (default), 'openai', or 'azure'", &provider_);
        add_string_flag(*cmd, "model", 'm', "Vision LLM model name (e.g. llava, gpt-4o)", &model_);
        add_string_flag(*cmd, "url", 'u',
                        "LLM base URL (default depends on --provider, e.g. "
                        "http://localhost:11434 for ollama)",
                        &base_url_);
        add_string_flag(*cmd, "language", 'l',
                        "Summary output language (e.g. 'ko', 'en'). Default: ko", &language_);

        cmd->handler = [this](const wcppcli::Command & /*unused*/) { return run_ocr(); };

        root.add_command(std::move(cmd));
    }

  private:
    auto run_ocr() -> int {
        if (file_path_.empty()) {
            wcppcli::WLog::error("--file (-f) is required. Specify an image file path.");
            return 1;
        }

        if (!std::filesystem::exists(file_path_)) {
            wcppcli::WLog::error("File not found: " + file_path_);
            return 1;
        }

        wcppcli::WConf conf;
        ragcli::utils::load_config(conf);

        llm::ProviderOverrides overrides{&provider_, &base_url_, &model_, nullptr};
        llm::ProviderTargets targets = llm::resolve_provider_targets(overrides, conf);

        // 1) 이미지 파일을 raw bytes 로 읽어 Base64 인코딩
        wcppcli::WLog::info("Reading image: " + file_path_);

        auto image_bytes = read_file_bytes(file_path_);
        if (image_bytes.empty()) {
            wcppcli::WLog::error("Failed to read image file or file is empty: " + file_path_);
            return 1;
        }

        const std::string mime_type = detect_mime_type(file_path_);
        const std::string image_base64 = ragcli::utils::base64_encode(image_bytes);

        wcppcli::WLog::info("Image size: " + std::to_string(image_bytes.size()) + " bytes (" +
                            mime_type + ")");
        wcppcli::WLog::info("Requesting OCR + summary from LLM (" + targets.provider + " / " +
                            targets.model + ")...");

        // 2) OCR + 요약 프롬프트 생성
        const std::string lang = language_.empty() ? "ko" : language_;
        std::string prompt = build_ocr_prompt(lang);

        // 3) Vision LLM 호출 (이미지 + 텍스트 멀티모달)
        auto llm_client = llm_client::LLMClientFactory::create(
            targets.provider, targets.api_key, targets.base_url, targets.api_version);

        llm_client::RequestParams gen_params;
        gen_params.model = targets.model;

        std::vector<llm_client::ContentBlock> blocks;
        blocks.push_back(llm_client::ContentBlock::makeText(prompt));
        blocks.push_back(llm_client::ContentBlock::makeImageBase64(image_base64, mime_type));

        llm_client::Message msg("user", blocks);

        std::cout << "\n[OCR + Summary]\n";
        bool any_chunk_streamed = false;
        llm_client::StreamCallback print_chunk = [&any_chunk_streamed](const std::string &chunk) {
            any_chunk_streamed = true;
            std::cout << chunk << std::flush;
        };

        try {
            auto response = llm_client->chatStream({msg}, print_chunk, gen_params);
            if (!any_chunk_streamed) {
                std::cout << response.content;
            }
            std::cout << std::endl;
        } catch (const std::exception &e) {
            wcppcli::WLog::error("LLM OCR failed: " + std::string(e.what()));
            return 1;
        }

        return 0;
    }

    // 이미지 파일의 raw bytes 를 읽어온다.
    static auto read_file_bytes(const std::string &path) -> std::vector<uint8_t> {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return {};
        }
        std::ostringstream oss;
        oss << file.rdbuf();
        std::string str = oss.str();
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    // 파일 확장자로부터 MIME 타입을 결정한다.
    static auto detect_mime_type(const std::string &path) -> std::string {
        const std::size_t dot_pos = path.rfind('.');
        if (dot_pos == std::string::npos) {
            return "image/jpeg";
        }
        std::string ext = path.substr(dot_pos);
        // 소문자 변환
        for (auto &c : ext) {
            c = static_cast<char>(std::tolower(c));
        }
        if (ext == ".png") {
            return "image/png";
        }
        if (ext == ".jpg" || ext == ".jpeg") {
            return "image/jpeg";
        }
        if (ext == ".gif") {
            return "image/gif";
        }
        if (ext == ".webp") {
            return "image/webp";
        }
        if (ext == ".bmp") {
            return "image/bmp";
        }
        return "image/jpeg";
    }

    // OCR + 요약용 프롬프트를 생성한다.
    static auto build_ocr_prompt(const std::string &language) -> std::string {
        std::string lang_instruction;
        if (language == "ko" || language == "한국어" || language == "korean") {
            lang_instruction = "한국어로";
        } else if (language == "en" || language == "english") {
            lang_instruction = "in English";
        } else {
            lang_instruction = "in " + language;
        }

        return std::string(
                   "첨부된 이미지를 분석하여 다음 작업을 수행해주세요.\n\n"
                   "1. **OCR (텍스트 추출)**: 이미지에 있는 모든 텍스트를 가능한 한 정확하게 "
                   "추출하세요. 영수증, 문서, 표, 라벨 등 형식에 맞게 구조적으로 정리하세요.\n"
                   "2. **요약 정리**: 추출된 텍스트를 바탕으로 핵심 내용을 ") +
               lang_instruction +
               " 요약해주세요. "
               "영수증의 경우 매장명, 날짜, 품목별 금액, 총액 등을 정리하고, "
               "문서의 경우 주요 포인트를 명확하게 요약하세요.\n\n"
               "출력 형식:\n"
               "--- OCR 결과 ---\n"
               "(추출된 전체 텍스트)\n\n"
               "--- 요약 ---\n"
               "(핵심 내용 요약)";
    }

    std::string file_path_;
    std::string model_;
    std::string base_url_;
    std::string provider_;
    std::string language_;
};

} // namespace ragcli::cmd