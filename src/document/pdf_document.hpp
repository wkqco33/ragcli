#pragma once

#include <algorithm>
#include <math.h>
#include <string>
#include <utility>
#include <vector>

#include "cpppdf/cpppdf.hpp"
#include "cpppdf/document.hpp"
#include "document/document_source.hpp"

namespace ragcli::document {

// cpppdf 를 이용해 PDF 의 텍스트와 이미지를 추출한다.
class PdfFileSource : public DocumentSource {
  public:
    explicit PdfFileSource(std::string path) : path_(std::move(path)) {}

    auto extract() const -> std::vector<ExtractedPage> override {
        cpppdf::PdfDocument doc;
        if (!doc.load(path_)) {
            throw std::runtime_error("Failed to load PDF: " + path_);
        }

        std::vector<ExtractedPage> all_pages;
        const int page_count = doc.page_count();
        all_pages.reserve(static_cast<size_t>(page_count) * 2);

        for (int i = 0; i < page_count; ++i) {
            auto pages = extract_page(doc, i);
            for (auto &page : pages) {
                all_pages.push_back(std::move(page));
            }
        }
        return all_pages;
    }

    auto source_name() const -> std::string override {
        return path_;
    }

  private:
    static auto extract_text_from_blocks(std::vector<cpppdf::TextBlock> blocks) -> std::string {
        // Y 내림차순, 동일 Y 에서는 X 오름차순 정렬.
        std::sort(blocks.begin(), blocks.end(),
                  [](const cpppdf::TextBlock &a, const cpppdf::TextBlock &b) {
                      if (std::abs(a.y - b.y) > 2.0F) {
                          return a.y > b.y;
                      }
                      return a.x < b.x;
                  });

        std::string text;
        float prev_y = -1.0e9F;
        for (const auto &block : blocks) {
            if (!text.empty() && prev_y >= 0.0F && std::abs(block.y - prev_y) > 4.0F) {
                text += '\n';
            } else if (!text.empty()) {
                text += ' ';
            }
            text += block.text;
            prev_y = block.y;
        }
        return text;
    }

    auto extract_page(const cpppdf::PdfDocument &doc, int page_index) const
        -> std::vector<ExtractedPage> {
        std::vector<ExtractedPage> pages;

        ExtractedPage text_page;
        text_page.page_index = page_index + 1;
        text_page.title = path_ + "#page=" + std::to_string(text_page.page_index);

        auto text_blocks =
            cpppdf::extract_text(&const_cast<cpppdf::PdfDocument &>(doc), page_index);
        text_page.text = extract_text_from_blocks(std::move(text_blocks));
        pages.push_back(std::move(text_page));

        auto images = cpppdf::extract_images(&const_cast<cpppdf::PdfDocument &>(doc), page_index);
        for (std::size_t img_idx = 0; img_idx < images.size(); ++img_idx) {
            const auto &img = images[img_idx];
            ExtractedPage image_page;
            image_page.page_index = page_index + 1;
            image_page.title = path_ + "#page=" + std::to_string(image_page.page_index) +
                               ".image." + std::to_string(img_idx + 1);
            image_page.text = "[Image " + std::to_string(img_idx + 1) + " from PDF page " +
                              std::to_string(image_page.page_index) + "]";
            image_page.image = img.pixels;
            image_page.image_width = img.width;
            image_page.image_height = img.height;
            image_page.is_image = true;
            pages.push_back(std::move(image_page));
        }

        return pages;
    }

    std::string path_;
};

} // namespace ragcli::document
