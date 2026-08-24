# AGENTS.md — ragcli 개발 가이드 (AI 에이전트용)

이 파일은 이 저장소에서 작업하는 AI 에이전트(및 사람 기여자)가 반드시 따라야 할 작업 규칙입니다.
개발 철학과 아키텍처 상세는 [DEVELOPMENT.md](DEVELOPMENT.md)를 참고하세요.

## 1. 테스트 주도 개발 (TDD) — 필수

이 프로젝트는 TDD를 기본 방식으로 합니다. 코드를 추가/수정할 때는 반드시 다음 순서를 따르세요.

1. **테스트 먼저 작성** — `tests/<대상>_test.cpp`에 실패하는 테스트를 먼저 추가합니다.
2. **테스트가 통과하도록 최소 구현** — `src/`의 해당 모듈을 구현/수정합니다.
3. **리팩터링** — 테스트가 여전히 통과하는지 확인하며 정리합니다.
4. **전체 테스트 실행** — 작업이 끝나면 아래 명령으로 전체 테스트가 통과하는지 확인하세요.

```bash
cmake --preset debug -DBUILD_TESTING=ON
cmake --build build/debug --target ragcli_tests
ctest --test-dir build/debug --output-on-failure
```

- 새 기능/커맨드를 추가했는데 테스트를 동반하지 않으면 완료로 간주하지 마세요.
- 테스트는 외부 자원(네트워크, 실제 LLM/Qdrant)에 의존하지 않아야 합니다. `rag::LlmPort`, `rag::QdrantPort` 인터페이스를 `tests/mock_qdrant_port.hpp` 같은 Mock으로 주입하세요.
- 버그 수정도 회귀 테스트를 먼저 추가하는 것을 원칙으로 합니다.

## 2. 작업 규칙

- **무단 대규모 리팩터링 금지** — 동작을 바꾸지 않는 리팩터링은 기존 테스트가 모두 통과한 상태에서만, 작게 나누어 수행하세요.
- **스코프 유지** — 요청받은 범위를 벗어난 파일을 건드리지 마세요. 관련이 있다면 먼저 제안하세요.
- **코드 스타일** — 커밋 전에 clang-format(`.clang-format`)을 적용하세요. `git clang-format` 또는 수동 정렬.
- **주석 원칙** — 코드를 되풀이하는 주석을 달지 마세요. 주석은 "왜(why)"를 설명하거나 비자명한 제약을 알릴 때만 간결하게 작성하세요. 장황한 설명/에이전트 독백 주석은 금지입니다.
- **시크릿 금지** — 실제 API 키, 토큰, 개인정보를 코드/커밋에 넣지 마세요. 항상 `.env.example`/`config.yaml.example`의 플레이스홀더 패턴을 사용하세요.
- **로그 파일 금지** — `action_err.log`, `*.log` 등 대용량 진단 로그를 저장소에 커밋하지 마세요 (`.gitignore`에 있음).

## 3. 새 커맨드 추가 시

- `src/cmd/`에 `CommandBase` 파생 헤더를 작성하고, `src/cmd/registry.hpp`에 등록합니다.
- `tests/registry_test.cpp` 및 `tests/<command>_test.cpp`에 플래그/동작 테스트를 추가합니다.
- README의 "서브커맨드 목록" 표에 문서화합니다.

## 4. 문서

- 사용자에게 노출되는 옵션/설정을 바꾸면 README.md, config.yaml.example, .env.example을 함께 갱신하세요.
- 이 파일과 DEVELOPMENT.md는 저장소의 진실 공급원(single source of truth)입니다.
