#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <wcppcli/wconf.hpp>
#include <wcppcli/wlog.hpp>

#include "chat/chat_config.hpp"
#include "command.hpp"
#include "document/document_source_factory.hpp"
#include "flag_helper.hpp"
#include "llm/provider_config.hpp"
#include "llm_client/llm_client_factory.hpp"
#include "rag/llm_port.hpp"
#include "utils/config_path.hpp"
#include "utils/utf8.hpp"

namespace ragcli::cmd {

// `ragcli pdf` 서브커맨드.
// PDF 파일을 읽어 텍스트를 추출한 뒤 LLM 에게 전달하여 요약 정리를 수행한다.
// 대용량 문서의 경우 Map-Reduce 단계별 요약을 수행하여 LLM 컨텍스트 한계 및 타임아웃을 방지한다.
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
        add_string_flag(*cmd, "pages", 'p', "Page ranges to include (e.g. '1-50', '1,3,5-10')",
                        &pages_);
        add_int_flag(*cmd, "max-chars", 0,
                     "Maximum text characters to extract and process (0: unlimited)", &max_chars_);
        add_int_flag(*cmd, "chunk-size", 0,
                     "Chunk character size for Map-Reduce phase (default: 15000)", &chunk_size_);
        add_bool_flag(*cmd, "map-reduce", 0,
                      "Force Map-Reduce chunked summarization (auto-enabled if text > 30000 chars)",
                      &map_reduce_);

        cmd->handler = [this](const wcppcli::Command & /*unused*/) { return run_summarize(); };

        root.add_command(std::move(cmd));
    }

    static auto parse_page_ranges(const std::string &spec) -> std::vector<std::pair<int, int>> {
        std::vector<std::pair<int, int>> ranges;
        if (spec.empty()) {
            return ranges;
        }
        std::stringstream ss(spec);
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (item.empty()) {
                continue;
            }
            auto dash = item.find('-');
            if (dash != std::string::npos) {
                try {
                    int start_p = std::stoi(item.substr(0, dash));
                    int end_p = std::stoi(item.substr(dash + 1));
                    ranges.emplace_back(start_p, end_p);
                } catch (...) {
                }
            } else {
                try {
                    int page = std::stoi(item);
                    ranges.emplace_back(page, page);
                } catch (...) {
                }
            }
        }
        return ranges;
    }

    static auto split_text_for_map_reduce(const std::string &text, std::size_t target_chunk_chars)
        -> std::vector<std::string> {
        std::vector<std::string> chunks;
        std::size_t total_bytes = text.size();
        std::size_t start = 0;

        while (start < total_bytes) {
            std::size_t window_end = utils::utf8::advance_chars(text, start, target_chunk_chars);
            std::size_t end = window_end;

            if (window_end < total_bytes) {
                std::size_t lookback = (std::max<std::size_t>)(target_chunk_chars / 5, 100);
                std::size_t lookback_start = utils::utf8::retreat_chars(text, window_end, lookback);
                if (lookback_start < start) {
                    lookback_start = start;
                }
                std::string_view search_window(text.data() + lookback_start,
                                               window_end - lookback_start);

                if (auto pos = search_window.rfind("\n\n"); pos != std::string_view::npos) {
                    end = lookback_start + pos + 2;
                } else if (auto pos = search_window.rfind('\n'); pos != std::string_view::npos) {
                    end = lookback_start + pos + 1;
                } else {
                    end = utils::utf8::snap_back(text, window_end);
                }
            }

            if (end <= start) {
                end = utils::utf8::advance_chars(text, start, target_chunk_chars);
            }

            chunks.push_back(text.substr(start, end - start));
            start = end;
        }

        return chunks;
    }

  private:
    auto run_summarize() -> int {
        if (file_path_.empty()) {
            wcppcli::WLog::error("--file (-f) is required. Specify a PDF file path.");
            return 1;
        }

        wcppcli::WConf conf;
        ragcli::utils::load_config(conf);

        // LLM 프로바이더 설정 해석 (chat 커맨드와 동일한 방식)
        llm::ProviderOverrides overrides{&provider_, &base_url_, &model_, nullptr};
        llm::ProviderTargets targets = llm::resolve_provider_targets(overrides, conf);

        // 1) PDF 텍스트 추출
        wcppcli::WLog::info("Extracting text from PDF: " + file_path_);
        auto source = document::create_source_from_path(file_path_);
        auto pages = source->extract();

        const auto page_ranges = parse_page_ranges(pages_);
        std::string full_text;
        int matched_pages = 0;

        for (const auto &page : pages) {
            if (!page.is_image && !page.text.empty()) {
                if (!page_ranges.empty()) {
                    bool in_range = false;
                    for (const auto &[start_p, end_p] : page_ranges) {
                        if (page.page_index >= start_p && page.page_index <= end_p) {
                            in_range = true;
                            break;
                        }
                    }
                    if (!in_range) {
                        continue;
                    }
                }
                matched_pages++;
                if (!full_text.empty()) {
                    full_text += "\n\n";
                }
                full_text += page.text;
            }
        }

        if (full_text.empty()) {
            wcppcli::WLog::error(
                "No text content could be extracted from the PDF matching criteria.");
            return 1;
        }

        if (!page_ranges.empty()) {
            wcppcli::WLog::info("Filtered " + std::to_string(matched_pages) +
                                " pages matching page ranges: " + pages_);
        }

        if (max_chars_ > 0 && full_text.size() > static_cast<std::size_t>(max_chars_)) {
            std::size_t truncated_bytes =
                utils::utf8::advance_chars(full_text, 0, static_cast<std::size_t>(max_chars_));
            wcppcli::WLog::info("Truncating extracted text from " +
                                std::to_string(full_text.size()) + " to " +
                                std::to_string(max_chars_) + " characters (--max-chars).");
            full_text = full_text.substr(0, truncated_bytes);
        }

        wcppcli::WLog::info("Extracted " + std::to_string(full_text.size()) +
                            " characters. Requesting summary from LLM (" + targets.provider +
                            " / " + targets.model + ")...");

        const std::string lang = language_.empty() ? "ko" : language_;
        auto llm_client = llm_client::LLMClientFactory::create(
            targets.provider, targets.api_key, targets.base_url, targets.api_version);

        llm_client::RequestParams gen_params;
        gen_params.model = targets.model;

        constexpr std::size_t k_map_reduce_threshold = 30000;
        const bool use_map_reduce = map_reduce_ || (full_text.size() > k_map_reduce_threshold);

        try {
            if (!use_map_reduce) {
                // 단일 프롬프트 스트리밍 요약
                std::string prompt = build_summary_prompt(full_text, lang);
                std::cout << "\n[Summary]\n";
                bool any_chunk_streamed = false;
                llm_client::StreamCallback print_chunk =
                    [&any_chunk_streamed](const std::string &chunk) {
                        any_chunk_streamed = true;
                        std::cout << chunk << std::flush;
                    };

                auto response = llm_client->generateStream(prompt, print_chunk, gen_params);
                if (!any_chunk_streamed) {
                    std::cout << response.content;
                }
                std::cout << std::endl;
            } else {
                // Map-Reduce 단계별 요약
                const std::size_t chunk_chars =
                    (chunk_size_ > 0) ? static_cast<std::size_t>(chunk_size_) : 15000;
                auto chunks = split_text_for_map_reduce(full_text, chunk_chars);

                wcppcli::WLog::info("Text size (" + std::to_string(full_text.size()) +
                                    " chars) exceeds threshold (" +
                                    std::to_string(k_map_reduce_threshold) +
                                    " chars). Using Map-Reduce chunked summarization with " +
                                    std::to_string(chunks.size()) + " chunks...");

                // 1) Map 단계: 각 청크별 요약 생성
                std::vector<std::string> chunk_summaries;
                chunk_summaries.reserve(chunks.size());

                for (std::size_t i = 0; i < chunks.size(); ++i) {
                    wcppcli::WLog::info("[Map " + std::to_string(i + 1) + "/" +
                                        std::to_string(chunks.size()) + "] Summarizing section (" +
                                        std::to_string(chunks[i].size()) + " chars)...");

                    std::string map_prompt =
                        build_map_prompt(chunks[i], i + 1, chunks.size(), lang);

                    std::string section_summary;
                    llm_client::StreamCallback capture_cb =
                        [&section_summary](const std::string &c) { section_summary += c; };

                    auto resp = llm_client->generateStream(map_prompt, capture_cb, gen_params);
                    if (section_summary.empty()) {
                        section_summary = resp.content;
                    }
                    chunk_summaries.push_back(section_summary);
                }

                // 2) Reduce 단계: 각 섹션 요약을 통합하여 최종 요약 생성
                wcppcli::WLog::info("[Reduce] Combining " + std::to_string(chunk_summaries.size()) +
                                    " section summaries into final summary...");

                std::string combined_summaries;
                for (std::size_t i = 0; i < chunk_summaries.size(); ++i) {
                    combined_summaries += "--- [섹션 " + std::to_string(i + 1) + " 요약] ---\n" +
                                          chunk_summaries[i] + "\n\n";
                }

                std::string reduce_prompt = build_reduce_prompt(combined_summaries, lang);

                std::cout << "\n[Summary]\n";
                bool any_chunk_streamed = false;
                llm_client::StreamCallback print_chunk =
                    [&any_chunk_streamed](const std::string &chunk) {
                        any_chunk_streamed = true;
                        std::cout << chunk << std::flush;
                    };

                auto response = llm_client->generateStream(reduce_prompt, print_chunk, gen_params);
                if (!any_chunk_streamed) {
                    std::cout << response.content;
                }
                std::cout << std::endl;
            }
        } catch (const std::exception &e) {
            wcppcli::WLog::error("LLM summarization failed: " + std::string(e.what()));
            return 1;
        }

        return 0;
    }

    static auto build_summary_prompt(const std::string &text,
                                     const std::string &language) -> std::string {
        std::string lang_instruction;
        if (language == "ko" || language == "한국어" || language == "korean") {
            lang_instruction = "요약은 한국어로 작성하세요. ";
        } else if (language == "en" || language == "english") {
            lang_instruction = "Write the summary in English. ";
        } else {
            lang_instruction = "Write the summary in " + language + ". ";
        }

        return std::string("아래 PDF 문서의 텍스트를 읽고 핵심 내용을 요약해주세요. ") +
               lang_instruction +
               "문서의 주요 주제, 핵심 포인트, 그리고 중요한 세부 사항을 포함하여 "
               "명확하고 간결하게 정리해주세요.\n\n"
               "--- 문서 내용 ---\n" +
               text +
               "\n--- 문서 내용 끝 ---\n\n"
               "위 문서에 대한 요약을 작성해주세요.";
    }

    static auto build_map_prompt(const std::string &text, std::size_t chunk_idx,
                                 std::size_t total_chunks,
                                 const std::string &language) -> std::string {
        std::string lang_instruction;
        if (language == "ko" || language == "한국어" || language == "korean") {
            lang_instruction = "요약은 한국어로 작성하세요. ";
        } else if (language == "en" || language == "english") {
            lang_instruction = "Write the summary in English. ";
        } else {
            lang_instruction = "Write the summary in " + language + ". ";
        }

        return "아래는 전체 문서의 일부(" + std::to_string(chunk_idx) + "/" +
               std::to_string(total_chunks) + ") 내용입니다. 이 구역의 핵심 내용을 요약해주세요. " +
               lang_instruction +
               "불필요한 인사말 없이 주요 내용만 명확히 요약해주세요.\n\n"
               "--- 섹션 내용 ---\n" +
               text + "\n--- 섹션 내용 끝 ---\n";
    }

    static auto build_reduce_prompt(const std::string &combined_summaries,
                                    const std::string &language) -> std::string {
        std::string lang_instruction;
        if (language == "ko" || language == "한국어" || language == "korean") {
            lang_instruction = "요약은 한국어로 작성하세요. ";
        } else if (language == "en" || language == "english") {
            lang_instruction = "Write the summary in English. ";
        } else {
            lang_instruction = "Write the summary in " + language + ". ";
        }

        return "아래는 문서의 각 구역별 요약 내용들입니다. 이 내용들을 바탕으로 전체 문서의 통합 "
               "요약본을 작성해주세요. " +
               lang_instruction +
               "문서의 전체적인 주요 주제, 핵심 포인트, 그리고 중요한 세부 사항을 논리적이고 "
               "깔끔한 구조(헤더, 글머리 기호 등)로 정리해주세요.\n\n"
               "--- 구역별 요약 모음 ---\n" +
               combined_summaries +
               "\n--- 구역별 요약 모음 끝 ---\n\n"
               "위 내용을 종합한 전체 요약을 작성해주세요.";
    }

    std::string file_path_;
    std::string model_;
    std::string base_url_;
    std::string provider_;
    std::string language_;
    std::string pages_;
    int max_chars_ = 0;
    int chunk_size_ = 0;
    bool map_reduce_ = false;
};

} // namespace ragcli::cmd