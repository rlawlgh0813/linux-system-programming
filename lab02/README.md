# Lab02 – SSU Cleanupd

지정한 디렉토리를 **지속적으로 모니터링하며 자동으로 파일을 정리하는 데몬 프로세스**를 구현하였다.
본 실습에서는 단순한 명령 실행 프로그램을 넘어, 백그라운드에서 동작하는 **Linux 데몬 프로세스 구조**,  프로세스 제어, 시그널 처리, 설정 파일 관리 및 로그 기록을 중심으로 시스템 프로그래밍을 학습하였다.

---

## 🔧 Implemented Features

### Built-in Commands
- `show`
- `add <DIR_PATH> [OPTION]`
- `modify <DIR_PATH> [OPTION]`
- `remove <DIR_PATH>`
- `help`
- `exit`


### `show`
- 현재 실행 중인 모든 데몬 프로세스 목록 출력
- 사용자 선택을 통해 데몬 상세 정보 조회
  - 설정 정보 (`ssu_cleanupd.config`)
  - 로그 정보 (`ssu_cleanupd.log`)
- 잘못된 입력에 대한 예외 처리


### `add`
- 지정한 디렉토리를 모니터링하는 데몬 프로세스 생성
- 주기적으로 디렉토리를 검사하여 정리되지 않은 파일 자동 분류

#### Supported Options
- `-d <OUTPUT_PATH>` : 정리 결과 디렉토리 지정
- `-i <TIME_INTERVAL>` : 모니터링 주기(초)
- `-l <MAX_LOG_LINES>` : 로그 최대 줄 수
- `-x <EXCLUDE_PATHS>` : 제외할 하위 디렉토리 지정
- `-e <EXTENSIONS>` : 정리할 확장자 지정
- `-m <MODE>` : 중복 파일 처리 방식
  - `1` : 최신 파일 유지
  - `2` : 오래된 파일 유지
  - `3` : 중복 파일 정리하지 않음

- 설정 정보는 `ssu_cleanupd.config` 파일로 관리
- 정리 내역은 `ssu_cleanupd.log` 파일에 기록


### `modify`
- 이미 등록된 데몬 프로세스의 설정 수정
- 설정 파일 갱신 후 `SIGHUP` 시그널을 통해 데몬 재로딩
- 수정하지 않은 옵션은 기존 값 유지


### `remove`
- 지정한 디렉토리를 모니터링 중인 데몬 프로세스 종료
- `SIGTERM` 전송 후 프로세스 정리
- 내부 데몬 목록에서 제거

---

## 🧠 Technical Highlights

- **데몬 프로세스 구현**
  - double-fork 방식
  - 세션 분리(`setsid`)
  - 표준 입출력 `/dev/null` 리다이렉션

- **프로세스 제어**
  - `fork()`, `execvp()`, `waitpid()`
  - `SIGTERM`, `SIGHUP` 시그널 처리

- **파일 시스템 처리**
  - `scandir()` 기반 디렉토리 순회
  - `stat`, `lstat`을 활용한 파일 정보 확인
  - 확장자 기반 파일 분류 및 정리

- **설정 및 로그 관리**
  - 설정 파일 파싱 및 재로딩
  - 로그 최대 줄 수 제한
  - 파일 락(`fcntl`)을 고려한 동시 접근 처리

- **경로 검증**
  - `realpath()` 기반 절대 경로 변환
  - HOME 디렉토리 외부 접근 차단
  - 중복·상위·하위 경로 충돌 방지

- `system()` 함수 미사용 (시스템 콜 기반 구현)

---

## ⚠️ Exception Handling

- 존재하지 않는 경로 입력
- 접근 권한이 없는 디렉토리
- HOME 디렉토리 외부 경로
- 이미 모니터링 중인 디렉토리
- 상위·하위 디렉토리 중복 등록
- 옵션 인자 형식 오류 및 범위 초과

---

## 🛠 Environment

- **OS**: Linux (Ubuntu)
- **Language**: C
- **Compiler**: gcc
- **Build Tool**: Makefile