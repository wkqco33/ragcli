#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "document/document_source.hpp"

namespace ragcli::document {

// 일반 텍스트 파일을 DocumentSource 로 노출한다.
class TextFileSource : public DocumentSource {
  public:
    explicit TextFileSource(std::string path) : path_(std::move(path)) {}

    auto extract() const -> std::vector<ExtractedPage> override {
        std::ifstream file(path_);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open text file: " + path_);
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        ExtractedPage page;
        page.text = buffer.str();
        page.title = path_;

        return {std::move(page)};
    }

    auto source_name() const -> std::string override {
        return path_;
    }

  private:
    std::string path_;
};

// 메모리 내 텍스트를 DocumentSource 로 노출한다.
class TextMemorySource : public DocumentSource {
  public:
    explicit TextMemorySource(std::string text, std::string name = "(memory)")
        : text_(std::move(text)), name_(std::move(name)) {}

    auto extract() const -> std::vector<ExtractedPage> override {
        ExtractedPage page;
        page.text = text_;
        page.title = name_;
        return {std::move(page)};
    }

    auto source_name() const -> std::string override {
        return name_;
    }

  private:
    std::string text_;
    std::string name_;
};

} // namespace ragcli::document
