# ragcli 개발 및 TDD 가이드

AI 개발 에이전트 및 기여자를 위한 ragcli 프로젝트 구조, 개발 철학, 테스트 작성 가이드입니다.

---

## 1. 개발 철학 및 아키텍처

- **탈객체지향 & 데이터 중심 구조**: 과도한 계층화나 추상화를 지양하고 데이터 구조체(`struct`)와 함수/클래스를 간단명료하게 설계합니다.
- **포트 & 어댑터 패턴 (디커플링)**:
  - 외부 서비스(LLM, Qdrant DB 등) 연결부는 `rag::LlmPort`, `rag::QdrantPort` 인터페이스로 격리되어 있습니다.
  - 이를 통해 네트워크 연동 없이 Mock 객체(`mock_qdrant_port.hpp`, `chat_runner_test.hpp`)로 빠른 단위 테스트가 가능합니다.
- **설정 로딩 (계층적 로드)**:
  - 로드 순서: **전역 설정 (`~/.config/ragcli/config.yaml` / `.env`)을 먼저 로드** → **CWD (`./config.yaml` / `./.env`)가 이어서 로드해 덮어씁니다 (Override)**.
  - 즉 우선순위는 높은 순으로 **CWD → 전역**이며, CWD의 설정값이 전역 설정을 덮어씁니다.

---

## 2. 프로젝트 모듈 구조

```text
src/
├── main.cpp                # 엔트리포인트 (WCLI 실행)
├── cmd/                    # CLI 커맨드 핸들러 (CommandBase 파생)
│   ├── command.hpp         # CommandBase 베이스
│   ├── flag_helper.hpp     # 플래그 등록 헬퍼
│   ├── chat.hpp            # ragcli chat
│   ├── collection.hpp      # ragcli collection
│   ├── config.hpp          # ragcli config (show, init, set, path)
│   ├── ocr.hpp             # ragcli ocr
│   ├── pdf_summarize.hpp   # ragcli pdf
│   ├── rag.hpp             # ragcli rag
│   └── registry.hpp        # 커맨드 등록 및 수명 관리
├── chunking/               # 텍스트 청킹 (Auto, Simple, Markdown, NoChunker)
├── document/               # 문서 데이터 소스 (PDF, Image, Text, Directory)
├── embedding/              # 임베딩 프로바이더
├── indexing/               # 인덱싱 엔진 (Indexer)
├── llm/                    # LLM 프로바이더 설정 및 분기
├── qdrant/                 # Qdrant REST 클라이언트
├── rag/                    # RAG 실행 로직 (RagRunner) 및 포트 정의
└── utils/                  # 공통 유틸리티 (config_path, base64, uuid)

> 외부 FetchContent 의존성: `wcppcli`(GitHub), `llm_client`(GitLab), `cpppdf`(GitHub)가
> CMakeLists.txt 의 FetchContent 로 받아와지며, vcpkg 로 cpr/spdlog/fmt/Stb/JPEG 를 설치한다.
> `llm_client/...` include 는 src/ 가 아니라 `build/_deps/llm_client-src/include` 에서 해석된다.

---

## 3. 테스트 작성 및 TDD 가이드

GoogleTest 기반으로 구현되어 있으며, 모든 주요 커맨드 및 실행기는 단위 테스트로 검증됩니다.

### 테스트 실행
```bash
# 디버그 빌드 및 테스트 타겟 생성
cmake --preset debug -DBUILD_TESTING=ON
cmake --build build/debug --target ragcli_tests

# 단위 테스트 실행 (전체 스위트)
./build/debug/tests/ragcli_tests

# CTest로 실행
ctest --test-dir build/debug --output-on-failure
```

### 새 기능 추가 시 TDD 수칙

1. **커맨드 추가시**:
   - `src/cmd/`에 커맨드 헤더 작성 (`CommandBase` 상속)
   - `src/cmd/registry.hpp`에 `holders.emplace_back(make_unique<...>());` 한 줄 추가
   - `tests/registry_test.cpp` 및 `tests/<command>_test.cpp`에 해당 커맨드 플래그 및 동작 검증 테스트 추가
2. **비즈니스 로직 추가시**:
   - `LlmPort`, `QdrantPort` 인터페이스를 mock으로 주입해 네트워크 통신 없이 단위 테스트를 작성합니다.
3. **독립성 & 속도 유지**:
   - 테스트 코드는 간단하고 명확하게 작성하며, 외부 자원에 의존하지 않도록 구성합니다.

---

## 4. 설정 관리 (`ragcli config`)

- `ragcli config`: 현재 활성화된 설정 내용 확인
- `ragcli config init`: OS 표준 디렉토리(`~/.config/ragcli/config.yaml`)에 기본 설정 파일 생성
- `ragcli config set KEY VALUE`: 특정 설정값 변경/추가
- `ragcli config path`: 현재 활성 설정 파일 경로 출력
