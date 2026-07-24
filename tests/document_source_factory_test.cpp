#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "document/directory_source.hpp"
#include "document/document_source_factory.hpp"
#include "document/image_source.hpp"
#include "document/pdf_document.hpp"
#include "document/text_document.hpp"

namespace {

namespace fs = std::filesystem;

class DocumentSourceFactoryTest : public ::testing::Test {
  protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "ragcli_document_source_factory_test";
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }

    void TearDown() override {
        fs::remove_all(dir_);
    }

    auto touch(const std::string &name) -> std::string {
        const fs::path path = dir_ / name;
        std::ofstream(path).close();
        return path.string();
    }

    fs::path dir_;
};

TEST_F(DocumentSourceFactoryTest, RoutesPdfExtensionToPdfFileSource) {
    auto source = ragcli::document::create_source_from_path(touch("doc.pdf"));
    EXPECT_NE(std::dynamic_pointer_cast<ragcli::document::PdfFileSource>(source), nullptr);
}

TEST_F(DocumentSourceFactoryTest, RoutesImageExtensionsToImageFileSource) {
    for (const char *name : {"a.png", "b.jpg", "c.jpeg", "d.gif", "e.webp", "f.bmp"}) {
        auto source = ragcli::document::create_source_from_path(touch(name));
        EXPECT_NE(std::dynamic_pointer_cast<ragcli::document::ImageFileSource>(source), nullptr)
            << "extension of " << name << " should route to ImageFileSource";
    }
}

TEST_F(DocumentSourceFactoryTest, RoutesUnknownExtensionToTextFileSource) {
    auto source = ragcli::document::create_source_from_path(touch("notes.md"));
    EXPECT_NE(std::dynamic_pointer_cast<ragcli::document::TextFileSource>(source), nullptr);
}

TEST_F(DocumentSourceFactoryTest, RoutesDirectoryToDirectorySource) {
    auto source = ragcli::document::create_source_from_path(dir_.string());
    EXPECT_NE(std::dynamic_pointer_cast<ragcli::document::DirectorySource>(source), nullptr);
}

} // namespace
