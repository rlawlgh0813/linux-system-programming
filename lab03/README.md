# Lab03 – SSU-EXT2

ext2 파일 시스템 디스크 이미지 파일을 직접 파싱하여 루트 디렉토리부터 하위 디렉토리 및 파일 구조를 탐색하는 쉘 프로그램 `ssu_ext2`를 구현하였다.
운영체제가 제공하는 파일 API나 ext2 전용 라이브러리를 사용하지 않고,  
on-disk 구조체를 직접 정의하고 raw 디스크 이미지를 분석함으로써  
Linux 파일 시스템의 내부 동작 원리를 깊이 있게 이해하는 것을 목표로 한다.

---

## 🔧 Implemented Features

### Built-in Commands
- `tree <PATH> [OPTION]`
- `print <PATH> [OPTION]`
- `help [COMMAND]`
- `exit`

프로그램 실행 시 ext2 이미지 파일을 인자로 받아 프롬프트 기반의 인터랙티브 쉘 형태로 동작한다.


### tree
- ext2 이미지 내부 디렉토리 구조를 트리 형태로 출력
- 루트 기준 상대 경로 탐색 지원
- `"."`, `".."`, `"lost+found"` 디렉토리 제외
- 옵션
  - `-r` : 하위 디렉토리 재귀 탐색
  - `-s` : 파일 및 디렉토리 크기 출력
  - `-p` : 파일 및 디렉토리 권한 출력
- 옵션 중복 사용 가능
- 디렉토리 및 파일 개수 요약 출력


### print
- ext2 이미지 내부 파일 내용을 표준 출력으로 출력
- inode의 블록 포인터를 직접 따라가며 데이터 읽기
- Direct / Single / Double / Triple Indirect block 지원
- 옵션
  - `-n <line_number>` : 지정한 줄 수만 출력


### help
- 전체 명령어 또는 특정 명령어 사용법 출력
- 등록되지 않은 명령어 입력 시 기본 도움말 출력


### exit
- 프로그램 정상 종료

---

## Technical Highlights

- **Raw ext2 파일 시스템 파싱**
  - 슈퍼블록(offset 1024) 직접 읽기
  - Magic number 검증 (`0xEF53`)
  - 블록 크기 및 inode 크기 계산

- **On-disk 구조체 직접 정의**
  - `ext2_super_block`
  - `ext2_group_desc`
  - `ext2_inode`
  - `ext2_dir_entry_2`
  - ext2 관련 라이브러리 미사용

- **디렉토리 탐색**
  - inode → data block → directory entry 순회
  - 경로 토큰화를 통한 단계적 inode 탐색
  - 트리 출력용 링크드 리스트 직접 구현

- **파일 읽기**
  - Direct block
  - Single / Double / Triple Indirect block 처리
  - 블록 단위 `lseek` + `read` 기반 데이터 출력

- **시스템 프로그래밍 제약 준수**
  - `system()` 함수 미사용
  - `libext2fs`, `ext2_fs.h` 등 외부 라이브러리 미사용

---

## Exception Handling

- ext2 이미지 인자 없이 실행 시 Usage 출력
- 존재하지 않는 경로 입력
- 디렉토리가 아닌 경로에 tree 명령 사용
- 파일이 아닌 경로에 print 명령 사용
- 잘못된 옵션 또는 옵션 인자 누락
- 빈 입력 처리

---

## Environment

- **OS**: Linux (Ubuntu)
- **Language**: C
- **Compiler**: gcc
- **Build Tool**: Makefile