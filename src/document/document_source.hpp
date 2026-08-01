#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ragcli::document {

// 추출된 문서 페이지 또는 청킹 단위.
struct ExtractedPage {
    std::string text;           // 페이지/섹션 텍스트
    std::string title;          // 추출된 제목 또는 파일명
    int page_index = 0;         // 페이지 번호 (텍스트의 경우 0)
    std::vector<uint8_t> image; // 이미지가 있을 경우 RGBA 데이터
    int image_width = 0;
    int image_height = 0;
    bool is_image = false;    // true 면 image 데이터가 메인 콘텐츠
    std::string source_path;  // 이 페이지가 속한 실제 파일 경로 (디렉터리 인덱싱 시 source_name 과 다를 수 있음)
};

// 입력 소스 추상화 (파일, 메모리, URL 등).
class DocumentSource {
  public:
    virtual ~DocumentSource() = default;

    // 소스로부터 페이지/섹션 단위로 콘텐츠를 추출한다.
    virtual auto extract() const -> std::vector<ExtractedPage> = 0;

    // 소스 식별자 (파일 경로 등) 를 반환한다.
    virtual auto source_name() const -> std::string = 0;
};

} // namespace ragcli::document
