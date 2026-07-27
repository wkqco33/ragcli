#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <wcppcli/wlog.hpp>

#include "document/document_source.hpp"
#include "document/image_source.hpp"
#include "document/pdf_document.hpp"
#include "document/text_document.hpp"

namespace ragcli::document {

// 디렉터리 내 지원 파일들을 재귀적으로 탐색해 DocumentSource 로 노출한다.
class DirectorySource : public DocumentSource {
  public:
    explicit DirectorySource(std::string path) : path_(std::move(path)) {}

    auto extract() const -> std::vector<ExtractedPage> override {
        std::vector<ExtractedPage> all_pages;

        std::error_code ec;
        auto options = std::filesystem::directory_options::skip_permission_denied;
        auto iter = std::filesystem::recursive_directory_iterator(path_, options, ec);

        if (ec) {
            wcppcli::WLog::error("Failed to access directory '" + path_ + "': " + ec.message());
            return all_pages;
        }

        for (auto end = std::filesystem::recursive_directory_iterator(); iter != end;
             iter.increment(ec)) {
            if (ec) {
                wcppcli::WLog::warn("Error traversing directory entry: " + ec.message());
                ec.clear();
                continue;
            }

            const auto &entry = *iter;
            if (!entry.is_regular_file(ec)) {
                continue;
            }

            const std::string ext = entry.path().extension().string();
            std::shared_ptr<DocumentSource> source;

            if (ext == ".txt" || ext == ".md") {
                source = std::make_shared<TextFileSource>(entry.path().string());
            } else if (ext == ".pdf") {
                source = std::make_shared<PdfFileSource>(entry.path().string());
            } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" ||
                       ext == ".webp" || ext == ".bmp") {
                // 확장자 목록은 document_source_factory.hpp 의 단일 파일(--image) 목록과
                // 동일하게 유지한다. stb_image 가 실제로 디코딩할 수 없는 형식(.webp 등)은
                // 아래 catch 블록의 결함 허용 로직이 경고 로그와 함께 건너뛴다.
                source = std::make_shared<ImageFileSource>(entry.path().string());
            } else {
                continue;
            }

            try {
                auto pages = source->extract();
                for (auto &page : pages) {
                    all_pages.push_back(std::move(page));
                }
            } catch (const std::exception &e) {
                wcppcli::WLog::warn("Failed to extract file '" + entry.path().string() +
                                    "': " + e.what());
            }
        }

        return all_pages;
    }

    auto source_name() const -> std::string override {
        return path_;
    }

  private:
    std::string path_;
};

} // namespace ragcli::document
