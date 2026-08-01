# ragcli

Qdrant 벡터 데이터베이스와 LLM(Ollama, OpenAI, Azure OpenAI)을 연동하여 지식 검색, 문서 인덱싱, 멀티모달(이미지/PDF) 처리 및 생성형 답변(RAG)을 수행하는 C++17 기반 CLI 도구입니다.

---

## ppm 설치

`ppm`으로 설치할 수 있도록 메타데이터(`ppm.json`)가 포함되어 있습니다.

```bash
ppm install wkqco33/ragcli
```

---

## 주요 기능

- **RAG (Retrieval-Augmented Generation)**: Qdrant에 저장된 지식을 임베딩 벡터 검색으로 추출하고 LLM을 통해 답변을 생성합니다.
- **멀티 프로바이더 LLM 지원**: Ollama(로컬), OpenAI, Azure OpenAI 중 선택하여 `rag`/`chat`에 사용할 수 있습니다 (`--provider` 또는 `LLM_PROVIDER`).
- **다양한 문서 및 멀티모달 인덱싱**:
  - **텍스트/파일/PDF**: PDF 페이지별 텍스트 및 모든 이미지 자동 추출 및 저장.
  - **이미지 인덱싱**: PNG, JPG, JPEG, BMP 이미지 디코딩, 메타데이터 추출 및 Base64 인코딩 후 Qdrant Payload 저장.
  - **디렉터리 자동 탐색**: 결함 허용(Fault-Tolerance) 기반의 디렉터리 재귀 인덱싱 (특정 파일 실패 시에도 중단 없이 계속 진행).
- **의미 단위 청킹(Chunking) 전략**:
  - **Auto Chunker** (기본값): 파일 확장자에 따라 아래 두 전략을 파일 단위로 자동 선택합니다.
  - **Simple Chunker**: 문단(`\n\n`) > 줄 > 문장 > 공백 순으로 의미 경계를 찾아 자르는 재귀 구분자 청킹. UTF-8 코드포인트 경계를 항상 지켜 한글 등 멀티바이트 문자가 깨지지 않습니다.
  - **Markdown Chunker**: 마크다운 헤딩(`#`, `##`, `###` 등) 기반 구조적 섹션 청킹. 헤딩 계층 경로(`heading_path`)를 보존하고, 섹션이 청크 크기를 넘으면 Simple Chunker로 재분할합니다.
  - 검색 시 히트 주변 청크를 확장(`--expand-neighbors`)하고 인접 청크를 자동 병합해 LLM에 더 넓고 중복 없는 문맥을 전달합니다.
  - 기본적으로 벡터 검색 결과를 top-k보다 넓게 가져온 뒤 로컬 BM25 키워드 점수와 RRF(Reciprocal Rank Fusion)로 재정렬(`--rerank lexical`, 기본값)해, 임베딩이 놓치기 쉬운 정확한 키워드/고유명사 질문의 검색 품질을 보완합니다.
- **인터랙티브 대화 (Chat)**: LLM 서버와 연동하여 CLI 터미널 환경에서 연속 대화를 수행합니다.
- **PDF 요약 (pdf)**: PDF 파일의 텍스트를 추출하여 LLM으로 핵심 내용을 요약 정리합니다. Qdrant 없이 단독으로 동작합니다.
- **이미지 OCR + 요약 (ocr)**: 영수증, 문서 스캔 등 이미지의 텍스트를 Vision LLM으로 추출(OCR)하고 핵심 내용을 요약 정리합니다. Qdrant 없이 단독으로 동작합니다.
- **스트리밍 응답**: `chat`과 `rag -q` 모두 답변을 토큰 단위로 즉시 화면에 출력해 응답을 기다리는 체감 시간을 줄입니다.
- **설정 관리 (config)**: `config.yaml` / `.env` 파일의 조회, 초기화(`init`), 값 설정(`set`), 경로 확인(`path`) 서브커맨드 지원.
- **계층적 환경 설정 우선순위**: CLI 플래그 > 현재 디렉터리(CWD) 설정 (`config.yaml`/`.env`) > 전역 사용자 설정 (`~/.config/ragcli/config.yaml`) > 코드 기본값 순서로 설정을 오버라이드합니다.

> 💡 **개발 및 TDD 가이드**: 다른 에이전트나 개발자는 [DEVELOPMENT.md](DEVELOPMENT.md)를 참고하세요.

---

## 서브커맨드 목록

### 1. `rag` (RAG 검색 및 지식 관리)

Qdrant 벡터 검색 기반 질의응답 및 지식 데이터/문서/이미지 인덱싱을 수행합니다.

| 플래그 | 단축키 | 설명 | 기본값 |
| :--- | :--- | :--- | :--- |
| `--query` | `-q` | RAG 질의응답을 위한 사용자 질문 | (없음) |
| `--add` | `-a` | Qdrant 컬렉션에 청킹 없이 바로 추가할 짧은 지식 텍스트 | (없음) |
| `--file` | `-f` | Qdrant에 청킹하여 인덱싱할 텍스트/마크다운 파일 경로 | (없음) |
| `--title` | (없음) | 추가/인덱싱할 지식의 제목 (`--file`과 함께 쓰면 모든 청크의 title을 덮어씀) | (없음) |
| `--pdf` | (없음) | Qdrant에 인덱싱할 PDF 파일 경로 | (없음) |
| `--dir` | (없음) | Qdrant에 재귀적으로 인덱싱할 디렉터리 경로 | (없음) |
| `--image` | (없음) | Qdrant에 인덱싱할 이미지 파일 경로 (`.png`, `.jpg`, `.bmp` 등) | (없음) |
| `--chunker` | (없음) | 청킹 전략 (`auto`, `simple`, `markdown`) | `auto` |
| `--chunk-size` | (없음) | 청크 크기 (문자 수) | `512` |
| `--chunk-overlap` | (없음) | 청크 간 오버랩 (문자 수) | `64` |
| `--distance` | (없음) | Qdrant 거리 함수 (`Cosine`, `Euclid`, `Dot`, `Manhattan`) | `Cosine` |
| `--top-k` | `-k` | 검색할 상위 텍스트 청크 개수 | `5` |
| `--score-threshold` | (없음) | Qdrant 검색 최소 점수 임계값 (`0.0` ~ `1.0`) | `0.0` (비활성화) |
| `--expand-neighbors` | (없음) | 검색 히트마다 앞뒤로 가져올 이웃 청크 개수 | `0` (비활성화) |
| `--rerank` | (없음) | 검색 결과 리랭킹 전략 (`lexical`, `none`) | `lexical` |
| `--rerank-candidates` | (없음) | 리랭킹 전 Qdrant에서 가져올 후보 개수 | `0` (자동: `top-k*4`, 최소 20) |
| `--provider` | (없음) | LLM 프로바이더 (`ollama`, `openai`, `azure`) | `ollama` |
| `--model` | `-m` | 사용 LLM 모델명 | 프로바이더별 기본값 |
| `--embed-model` | `-e` | 임베딩 모델명 | 프로바이더별 기본값 |
| `--llm-url` | (없음) | LLM 서버 URL | 프로바이더별 기본값 |
| `--qdrant-url` | (없음) | Qdrant 서버 URL | `http://localhost:6333` |
| `--collection` | `-c` | Qdrant 컬렉션명 | `documents` |

#### 사용 예시

```bash
# 1) 짧은 텍스트를 청킹 없이 지식으로 추가
ragcli rag -a "삼성전자의 현재 주가는 75,000원입니다." --title "주가 정보"

# 2) 마크다운 문서 인덱싱 (확장자가 .md 이므로 --chunker auto 기본값이 마크다운 전략을 자동 선택)
ragcli rag --file ./docs/architecture.md

# 3) PDF 파일 인덱싱 (텍스트 + 모든 페이지 이미지 추출 및 Base64 Qdrant 저장)
ragcli rag --pdf ./documents/sample.pdf

# 4) 디렉터리 재귀 인덱싱 (파일마다 알맞은 청킹 전략 자동 적용, 개별 파일 오류 시에도 계속 진행)
ragcli rag --dir ./knowledge_base/

# 5) 단일 이미지 인덱싱
ragcli rag --image ./charts/diagram.png

# 6) 질문 및 RAG 답변 생성 (로컬 Ollama, 히트마다 이웃 청크 1개씩 확장, 리랭킹은 기본 활성화)
ragcli rag -q "삼성전자 주가가 얼마인가요?" -k 5 --expand-neighbors 1

# 6-1) 리랭킹 없이 순수 벡터 검색만 사용
ragcli rag -q "삼성전자 주가가 얼마인가요?" --rerank none

# 7) OpenAI를 프로바이더로 사용해 질의 (OPENAI_API_KEY는 .env 또는 환경 변수로 설정)
ragcli rag -q "삼성전자 주가가 얼마인가요?" --provider openai --model gpt-4o-mini
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

LLM 모델과 터미널에서 실시간 인터랙티브 대화를 수행합니다.

| 플래그 | 단축키 | 설명 | 기본값 |
| :--- | :--- | :--- | :--- |
| `--provider` | (없음) | LLM 프로바이더 (`ollama`, `openai`, `azure`) | `ollama` |
| `--model` | `-m` | 대화에 사용할 LLM 모델명 | 프로바이더별 기본값 |
| `--url` | `-u` | LLM 서버 URL | 프로바이더별 기본값 |

#### 사용 예시

```bash
# 로컬 Ollama
ragcli chat -m llama3 -u http://localhost:11434

# OpenAI (OPENAI_API_KEY는 .env 또는 환경 변수로 설정)
ragcli chat --provider openai -m gpt-4o-mini
```

---

### 4. `pdf` (PDF 요약)

PDF 파일을 읽어 텍스트를 추출한 뒤 LLM을 통해 핵심 내용을 요약 정리합니다. Qdrant나 임베딩 없이 순수하게 LLM 한 번 호출로 요약을 수행합니다.

| 플래그 | 단축키 | 설명 | 기본값 |
| :--- | :--- | :--- | :--- |
| `--file` | `-f` | 요약할 PDF 파일 경로 | (없음, 필수) |
| `--provider` | (없음) | LLM 프로바이더 (`ollama`, `openai`, `azure`) | `ollama` |
| `--model` | `-m` | 사용 LLM 모델명 | 프로바이더별 기본값 |
| `--url` | `-u` | LLM 서버 URL | 프로바이더별 기본값 |
| `--language` | `-l` | 요약 출력 언어 (`ko`, `en` 등) | `ko` |

#### 사용 예시

```bash
# 1) 로컬 Ollama로 PDF 요약 (기본 한국어)
ragcli pdf -f ./documents/report.pdf

# 2) OpenAI를 사용해 영어로 요약
ragcli pdf -f ./documents/report.pdf --provider openai -m gpt-4o-mini -l en

# 3) Ollama 모델 및 URL 직접 지정
ragcli pdf -f ./docs/paper.pdf -m llama3 -u http://localhost:11434
```

---

### 5. `ocr` (이미지 OCR + 요약)

영수증, 문서 스캔, 표 등 이미지 파일의 텍스트를 Vision LLM으로 추출(OCR)하고 핵심 내용을 요약 정리합니다. Qdrant나 임베딩 없이 LLM 한 번 호출로 동작합니다. Vision 기능이 지원되는 모델(예: `llava`, `gpt-4o` 등)을 사용해야 합니다.

| 플래그 | 단축키 | 설명 | 기본값 |
| :--- | :--- | :--- | :--- |
| `--file` | `-f` | OCR 할 이미지 파일 경로 (`.png`, `.jpg`, `.jpeg`, `.gif`, `.webp`, `.bmp`) | (없음, 필수) |
| `--provider` | (없음) | LLM 프로바이더 (`ollama`, `openai`, `azure`) | `ollama` |
| `--model` | `-m` | Vision LLM 모델명 (예: `llava`, `gpt-4o`) | 프로바이더별 기본값 |
| `--url` | `-u` | LLM 서버 URL | 프로바이더별 기본값 |
| `--language` | `-l` | 요약 출력 언어 (`ko`, `en` 등) | `ko` |

#### 사용 예시

```bash
# 1) 로컬 Ollama(llava)로 영수증 이미지 OCR + 요약 (기본 한국어)
ragcli ocr -f ./receipts/store_receipt.jpg -m llava

# 2) OpenAI GPT-4o를 사용해 영어로 OCR + 요약
ragcli ocr -f ./documents/scan.png --provider openai -m gpt-4o -l en

# 3) Ollama URL 및 모델 직접 지정
ragcli ocr -f ./charts/table.jpg -m llava -u http://localhost:11434
```

---

### 6. `config` (설정 관리)

`config.yaml` 또는 `.env` 설정 파일의 조회, 기본 파일 생성, 설정값 수정 및 경로 확인을 관리합니다.

| 서브커맨드 | 설명 |
| :--- | :--- |
| `ragcli config` | 현재 적용된 설정 파일의 전체 경로와 내용 출력 |
| `ragcli config init` | OS 표준 설정 디렉터리(`~/.config/ragcli/config.yaml` 등)에 기본 설정 파일 생성 |
| `ragcli config set KEY VALUE` | 지정한 키의 설정값을 파일에 업데이트/추가 |
| `ragcli config path` | 활성화된 설정 파일의 절대 경로 출력 |

#### 사용 예시

```bash
# 1) 기본 설정 파일 생성
ragcli config init

# 2) 프로바이더 및 모델 설정
ragcli config set LLM_PROVIDER ollama
ragcli config set OLLAMA_MODEL gemma4:31b-cloud

# 3) 현재 로드된 설정 확인
ragcli config
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
ctest --test-dir build/debug -R "^(RegisterCommands|RagCommand|RagRunner|ChatCommand|ChatRunner|Indexer|ImageFileSource|MarkdownChunker|Base64|PdfSummarize)" --output-on-failure
```

---

## 환경 변수 설정 (`.env`)

프로젝트 루트 디렉터리에 `.env` 파일을 작성하여 기본 URL 및 모델 정보를 설정할 수 있습니다. 예시는
`.env.example`을 참고하세요.

```env
# LLM_PROVIDER: ollama(기본값) | openai | azure
LLM_PROVIDER=ollama

OLLAMA_BASE_URL=http://localhost:11434
OLLAMA_MODEL=llama3
OLLAMA_EMBED_MODEL=nomic-embed-text

# LLM_PROVIDER=openai 일 때 사용
OPENAI_API_KEY=your_openai_api_key_here

# LLM_PROVIDER=azure 일 때 사용
AZURE_OPENAI_API_KEY=your_azure_openai_api_key_here
AZURE_OPENAI_BASE_URL=https://your-resource-name.openai.azure.com/
AZURE_OPENAI_MODEL=your-chat-deployment-name
AZURE_OPENAI_EMBED_MODEL=your-embedding-deployment-name

QDRANT_BASE_URL=http://localhost:6333
QDRANT_COLLECTION=documents
# QDRANT_DISTANCE=Cosine   # Cosine(기본값) | Euclid | Dot | Manhattan

# Simple/Markdown Chunker 튜닝 (문자 수 기준)
# CHUNK_SIZE=512
# CHUNK_OVERLAP=64

# ragcli rag -q 질의 시 히트마다 앞뒤로 가져올 이웃 청크 개수 (기본값 0=비활성화)
# EXPAND_NEIGHBORS=0

# 검색 결과 리랭킹: lexical(기본) | none. 로컬 BM25 + RRF 융합, 추가 네트워크/LLM 호출 없음
# RERANK_MODE=lexical
# RERANK_CANDIDATES=0
```
