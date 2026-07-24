# ragcli

Qdrant 벡터 데이터베이스와 Ollama LLM을 연동하여 지식 검색, 문서 인덱싱, 멀티모달(이미지/PDF) 처리 및 생성형 답변(RAG)을 수행하는 C++17 기반 CLI 도구입니다.

---

## 주요 기능

- **RAG (Retrieval-Augmented Generation)**: Qdrant에 저장된 지식을 임베딩 벡터 검색으로 추출하고 Ollama LLM을 통해 답변을 생성합니다.
- **다양한 문서 및 멀티모달 인덱싱**:
  - **텍스트/파일/PDF**: PDF 페이지별 텍스트 및 모든 이미지 자동 추출 및 저장.
  - **이미지 인덱싱**: PNG, JPG, JPEG, BMP 이미지 디코딩, 메타데이터 추출 및 Base64 인코딩 후 Qdrant Payload 저장.
  - **디렉터리 자동 탐색**: 결함 허용(Fault-Tolerance) 기반의 디렉터리 재귀 인덱싱 (특정 파일 실패 시에도 중단 없이 계속 진행).
- **다양한 청킹(Chunking) 전략**:
  - **Simple Chunker**: 고정 크기 + 오버랩 텍스트 청킹.
  - **Markdown Chunker**: 마크다운 헤딩(`#`, `##`, `###` 등) 기반의 구조적 섹션 청킹.
- **인터랙티브 대화 (Chat)**: Ollama 서버와 연동하여 CLI 터미널 환경에서 연속 대화를 수행합니다.
- **견고한 에러 핸들링 & 최상위 예외 관리**: 글로벌 Exception Handler, CPR 네트워크 및 JSON 파싱 유효성 검증 강화.
- **환경 설정 우선순위**: CLI 플래그 > 환경 변수 (`.env`) > 코드 기본값 순서로 설정을 유연하게 적용합니다.

---

## 서브커맨드 목록

### 1. `rag` (RAG 검색 및 지식 관리)

Qdrant 벡터 검색 기반 질의응답 및 지식 데이터/문서/이미지 인덱싱을 수행합니다.

| 플래그 | 단축키 | 설명 | 기본값 |
| :--- | :--- | :--- | :--- |
| `--query` | `-q` | RAG 질의응답을 위한 사용자 질문 | (없음) |
| `--add` | `-a` | Qdrant 컬렉션에 추가할 지식 텍스트 | (없음) |
| `--file` | `-f` | 추가할 지식 텍스트 파일 경로 | (없음) |
| `--title` | (없음) | 추가할 지식의 제목 | (없음) |
| `--pdf` | (없음) | Qdrant에 인덱싱할 PDF 파일 경로 | (없음) |
| `--dir` | (없음) | Qdrant에 재귀적으로 인덱싱할 디렉터리 경로 | (없음) |
| `--image` | (없음) | Qdrant에 인덱싱할 이미지 파일 경로 (`.png`, `.jpg`, `.bmp` 등) | (없음) |
| `--chunker` | (없음) | 청킹 전략 (`simple` 또는 `markdown`) | `simple` |
| `--top-k` | `-k` | 검색할 상위 텍스트 청크 개수 | `3` |
| `--score-threshold` | (없음) | Qdrant 검색 최소 점수 임계값 (`0.0` ~ `1.0`) | `0.0` (비활성화) |
| `--model` | `-m` | 사용 LLM 모델명 | `llama3` |
| `--embed-model` | `-e` | 임베딩 모델명 | `nomic-embed-text` |
| `--llm-url` | (없음) | Ollama LLM 서버 URL | `http://localhost:11434` |
| `--qdrant-url` | (없음) | Qdrant 서버 URL | `http://localhost:6333` |
| `--collection` | `-c` | Qdrant 컬렉션명 | `documents` |

#### 사용 예시

```bash
# 1) 텍스트로 지식 추가
ragcli rag -a "삼성전자의 현재 주가는 75,000원입니다." --title "주가 정보"

# 2) 마크다운 헤딩 기반 청킹으로 문서 인덱싱
ragcli rag --file ./docs/architecture.md --chunker markdown

# 3) PDF 파일 인덱싱 (텍스트 + 모든 페이지 이미지 추출 및 Base64 Qdrant 저장)
ragcli rag --pdf ./documents/sample.pdf

# 4) 디렉터리 재귀 인덱싱 (개별 파일 오류 발생 시에도 계속 진행)
ragcli rag --dir ./knowledge_base/ --chunker markdown

# 5) 단일 이미지 인덱싱
ragcli rag --image ./charts/diagram.png

# 6) 질문 및 RAG 답변 생성
ragcli rag -q "삼성전자 주가가 얼마인가요?" -k 5
```

---

### 2. `collection` (Qdrant 컬렉션 관리)

Qdrant 컬렉션 목록 조회, 상세 설정 확인 및 삭제를 관리합니다.

| 플래그 | 단축키 | 설명 | 기본값 |
| :--- | :--- | :--- | :--- |
| `--list` | `-l` | 모든 Qdrant 컬렉션 목록 조회 | `false` |
| `--info` | `-i` | 특정 컬렉션의 세부 정보 및 설정 조회 | `false` |
| `--delete` | `-d` | 지정한 Qdrant 컬렉션 삭제 | `false` |
| `--collection` | `-c` | 대상 컬렉션명 | `documents` |
| `--qdrant-url` | (없음) | Qdrant 서버 URL | `http://localhost:6333` |

#### 사용 예시

```bash
# 1) 전체 컬렉션 목록 조회
ragcli collection -l

# 2) 특정 컬렉션 상세 정보 조회
ragcli collection -i -c documents

# 3) 컬렉션 삭제
ragcli collection -d -c test_collection
```

---

### 3. `chat` (대화 세션)

Ollama LLM 모델과 터미널에서 실시간 인터랙티브 대화를 수행합니다.

| 플래그 | 단축키 | 설명 | 기본값 |
| :--- | :--- | :--- | :--- |
| `--model` | `-m` | 대화에 사용할 Ollama 모델명 | `llama3` |
| `--url` | `-u` | Ollama 서버 URL | `http://localhost:11434` |

#### 사용 예시

```bash
ragcli chat -m llama3 -u http://localhost:11434
```

---

### 4. `greet` (인사 및 데모)

기본 환영 메시지를 출력하는 데모 커맨드입니다.

```bash
ragcli greet --name "Developer"
```

---

## 빌드 및 실행

### 필수 조건

- **C++ Compiler**: C++17 이상 지원 (GCC 10+, Clang 11+, MSVC 2019+)
- **CMake**: 3.20 이상
- **Package Manager**: `vcpkg`

### 빌드 명령어

```bash
# VCPKG_ROOT 환경변수 설정
export VCPKG_ROOT=/path/to/vcpkg

# CMake 설정 및 빌드
cmake --preset debug -DBUILD_TESTING=ON
cmake --build build/debug --target ragcli ragcli_tests
```

---

## 테스트 실행

GoogleTest 기반 단위 테스트가 커맨드별로 분리되어 작성되어 있습니다 (`tests/`).

```bash
ctest --test-dir build/debug -R "^(RegisterCommands|GreetCommand|RagCommand|RagRunner|ChatCommand|ChatRunner|Indexer|ImageFileSource|MarkdownChunker|Base64)" --output-on-failure
```

---

## 환경 변수 설정 (`.env`)

프로젝트 루트 디렉터리에 `.env` 파일을 작성하여 기본 URL 및 모델 정보를 설정할 수 있습니다.

```env
OLLAMA_BASE_URL=http://localhost:11434
OLLAMA_MODEL=llama3
OLLAMA_EMBED_MODEL=nomic-embed-text
QDRANT_BASE_URL=http://localhost:6333
QDRANT_COLLECTION=documents
```
