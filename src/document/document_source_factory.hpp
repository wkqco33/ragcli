#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "document/directory_source.hpp"
#include "document/document_source.hpp"
#include "document/image_source.hpp"
#include "document/pdf_document.hpp"
#include "document/text_document.hpp"

namespace ragcli::document {

// 경로 확장자에 따라 적절한 DocumentSource 를 생성한다.
inline auto create_source_from_path(const std::string &path) -> std::shared_ptr<DocumentSource> {
    if (std::filesystem::is_directory(path)) {
        return std::make_shared<DirectorySource>(path);
    }

    const std::size_t dot_pos = path.rfind('.');
    std::string ext;
    if (dot_pos != std::string::npos) {
        ext = path.substr(dot_pos);
    }

    if (ext == ".pdf") {
        return std::make_shared<PdfFileSource>(path);
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp" ||
        ext == ".bmp") {
        return std::make_shared<ImageFileSource>(path);
    }

    // 기본적으로 텍스트 파일로 처리.
    return std::make_shared<TextFileSource>(path);
}

} // namespace ragcli::document
