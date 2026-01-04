# Lab01 – SSU Cleanup

Linux 환경에서 동작하는 **사용자 정의 쉘 프로그램**을 구현하였다.
디렉토리 구조를 트리 형태로 출력하고, 디렉토리 내 파일을 **확장자 기준으로 정리(arrange)** 하는 기능을 제공한다.  
본 실습을 통해 Linux 파일 시스템과 시스템 콜 기반 프로그래밍 흐름을 학습하였다.

---

## 🔧 Implemented Features

### Built-in Commands
- `tree <DIR_PATH> [OPTION]`
- `arrange <DIR_PATH> [OPTION]`
- `help`
- `exit`

### `tree`
- 디렉토리 구조를 트리 형태로 재귀 출력
- 사전 순 정렬(`scandir`)
- 상대 / 절대 경로 지원
- HOME 디렉토리 내부 경로만 허용
- 옵션
  - `-s` : 파일 크기 출력
  - `-p` : 파일 권한 출력
  - `-sp` : 크기 + 권한 출력

### `arrange`
- 디렉토리 내 파일을 확장자 기준으로 분류
- `<DIR_PATH>_arranged` 디렉토리 생성
- 옵션
  - `-d` : 결과 디렉토리 이름 지정
  - `-t` : 최근 수정 파일만 정리
  - `-x` : 제외 디렉토리 지정
  - `-e` : 특정 확장자만 정리
- 동일 파일명 존재 시 사용자 선택 기반 충돌 처리
  - `select`, `diff`, `vi`, `do not select`

---

## 🧠 Technical Highlights

- `scandir()` 기반 디렉토리 탐색 및 정렬
- 재귀 호출을 통한 트리 구조 구성
- `stat`, `lstat`을 활용한 파일 정보 처리
- `fork()`, `execvp()`, `waitpid()`를 이용한 프로세스 제어
- 사용자 입력 파싱 및 명령 분기 처리
- `system()` 함수 미사용 (시스템 콜 기반 구현)

---

## ⚠️ Exception Handling

- 존재하지 않는 경로 입력
- 디렉토리가 아닌 경로 입력
- HOME 디렉토리 외부 접근
- 지원하지 않는 옵션 입력
- 경로 및 파일 이름 길이 제한 초과

---

## 🛠 Environment

- **OS**: Linux (Ubuntu)
- **Language**: C
- **Compiler**: gcc
- **Build Tool**: Makefile