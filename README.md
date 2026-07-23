# ragcli

Qdrant 벡터 데이터베이스와 Ollama LLM을 연동하여 지식 검색 및 생성형 답변(RAG)을 수행하는 C++17 기반 CLI 도구입니다.

---

## 주요 기능

- **RAG (Retrieval-Augmented Generation)**: Qdrant에 저장된 지식을 임베딩 벡터 검색으로 추출하고 Ollama LLM을 통해 답변을 생성합니다.
- **지식 추가 (Add Knowledge)**: 텍스트 및 파일을 nomic-embed-text 모델로 임베딩하여 Qdrant 컬렉션에 바로 저장합니다.
- **인터랙티브 대화 (Chat)**: Ollama 서버와 연동하여 CLI 터미널 환경에서 연속 대화를 수행합니다.
- **환경 설정 우선순위**: CLI 플래그 > 환경 변수 (`.env`) > 코드 기본값 순서로 설정을 유연하게 적용합니다.

---

## 서브커맨드 목록

### 1. `rag` (RAG 검색 및 지식 관리)

Qdrant 벡터 검색 기반 질의응답 및 지식 데이터 추가를 수행합니다.

| 플래그 | 단축키 | 설명 | 기본값 |
| :--- | :--- | :--- | :--- |
| `--query` | `-q` | RAG 질의응답을 위한 사용자 질문 | (없음) |
| `--add` | `-a` | Qdrant 컬렉션에 추가할 지식 텍스트 | (없음) |
| `--file` | `-f` | 추가할 지식 텍스트 파일 경로 | (없음) |
| `--title` | (없음) | 추가할 지식의 제목 | (없음) |
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

# 2) 파일로 지식 추가
ragcli rag -f ./data/manual.txt --title "사용자 매뉴얼"

# 3) 질문 및 RAG 답변 생성
ragcli rag -q "삼성전자 주가가 얼마인가요?" -k 5

# 4) Qdrant 서버 및 컬렉션 지정
ragcli rag -q "오늘 날씨" --qdrant-url http://172.30.1.55:6333 -c documents_ollama
```

---

### 2. `chat` (대화 세션)

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

### 3. `greet` (인사 및 데모)

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
cmake --build build/debug
```

---

## 테스트 실행

GoogleTest 기반 테스트 코드가 커맨드별로 분리되어 작성되어 있습니다 (`tests/`).

```bash
ctest --test-dir build/debug --output-on-failure
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
