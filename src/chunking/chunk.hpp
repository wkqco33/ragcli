#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ragcli::chunking {

// 청킹 결과 단위.
struct Chunk {
    std::string text;      // 임베딩할 텍스트
    std::string title;     // 메타데이터용 제목
    std::string source;    // 출처 (파일 경로 등)
    int page_index = 0;    // 페이지 번호
    int chunk_index = 0;   // 소스 내 청크 번호
    bool is_image = false; // 이미지 콘텐츠 여부
    int image_width = 0;
    int image_height = 0;
    std::vector<std::uint8_t> image; // 원본 이미지 데이터 (있는 경우)
    std::string image_base64;        // Base64 인코딩된 이미지 데이터
};

} // namespace ragcli::chunking
