# Security Policy

## Supported Versions

현재 배포된 최신 태그(릴리스)에 대해서만 보안 수정을 제공합니다.

| Version | Supported |
| :--- | :--- |
| latest release | ✅ |

## Reporting a Vulnerability

보안 취약점을 발견하면 **공개 Issue에 작성하지 말고**, GitHub 저장소의
**Security → Report a vulnerability** 기능으로 비공개 보고를 부탁드립니다.

- 어떤 영향이 있는지(예: 원격 코드 실행, 정보 노출)를 포함해 주세요.
- 가능하면 재현 방법/버전을 함께 알려주세요.
- 확인 후 보안 패치를 최신 릴리스에 반영하고 보고자를 알려드립니다.

## 보안 원칙

- **시크릿 관리**: 실제 API 키/토큰을 코드나 저장소에 커밋하지 마세요.
  `OPENAI_API_KEY` 같은 값은 반드시 환경 변수 또는 `config.yaml`/`.env`로 주입합니다.
  `ragcli config` 명령은 설정을 표시할 때 API 키와 토큰 값을 마스킹합니다.
- **검증된 종속성**: 외부 라이브러리는 vcpkg/FetchContent를 통해 고정 버전으로 사용합니다.

## Release Integrity

공식 릴리스는 GitHub Releases에서 제공하며, 각 압축 파일에 대한 SHA-256
checksum을 함께 게시합니다. 배포 파일을 사용하기 전에 checksum을 확인하세요.
