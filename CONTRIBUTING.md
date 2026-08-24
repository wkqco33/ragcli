# Contributing to ragcli

ragcli에 기여해 주셔서 감사합니다. 아래 가이드를 따라 주시면 원활하게 협업할 수 있습니다.

## 개발 환경

- C++17, CMake ≥ 3.20, vcpkg (의존성: cpr, spdlog, fmt, Stb, JPEG)
- 모든 개발/기여자는 [AGENTS.md](AGENTS.md)와 [DEVELOPMENT.md](DEVELOPMENT.md)를 읽고 작업 방식을 따라주세요.

## 작업 절차 (TDD)

이 프로젝트는 **테스트 주도 개발(TDD)**을 기본으로 합니다.

1. 작업할 이슈나 제안이 있다면 먼저 Issue를 열어 논의하세요.
2. 저장소를 fork 하고 브랜치를 만드세요: `git checkout -b feature/description`
3. `tests/`에 실패하는 테스트를 먼저 작성하고, `src/`에서 구현하세요.
4. 전체 테스트가 통과하는지 확인하세요:

   ```bash
   cmake --preset debug -DBUILD_TESTING=ON
   cmake --build build/debug --target ragcli_tests
   ctest --test-dir build/debug --output-on-failure
   ```

5. clang-format(`.clang-format`)을 적용하세요.
6. 변경 사항을 검토 가능한 단위로 나누어 커밋하세요.

## 커밋 메시지

명확하고 간결하게 작성하세요. 예: `feat: support Azure OpenAI embedding`, `fix: prevent UTF-8 boundary split in simple chunker`.

## Pull Request 가이드

- PR은 하나의 논리적 변경에 집중해 주세요.
- 변경 사항과 테스트 결과를 설명에 요약해 주세요.
- 외부 서비스(LLM/Qdrant)에 의존하지 않는 단위 테스트를 포함해 주세요.

## 코드 스타일

- 4-space 들여쓰기, 100열 제한 (`.clang-format` 참조).
- 주석은 "왜"를 설명하는 데만 간결하게 사용하고, 코드를 되풀이하지 마세요.

## 보안

취약점 발견 시 GitHub의 **Security advisory** 기능(비공개) 또는 [SECURITY.md](SECURITY.md)의 연락처로 보고해 주세요. 공개 Issue에 시크릿을 올리지 마세요.
