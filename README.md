# Linux System Programming

Linux 환경에서 파일 시스템, 프로세스 제어, 데몬 프로세스, ext2 파일 시스템 내부 구조를 직접 구현하며 정리한 학습 저장소입니다.

운영체제가 제공하는 추상화 위에서 단순히 API를 사용하는 것이 아니라, 디렉토리 탐색, 프로세스 생성, 시그널 처리, 설정/로그 관리, raw disk image parsing을 직접 구현하며 Linux 시스템 프로그래밍의 동작 방식을 확인했습니다.

## Overview

| Lab | Project | What I implemented |
| --- | --- | --- |
| [lab01](lab01) | SSU Cleanup | 사용자 정의 shell, 디렉토리 tree 출력, 확장자 기반 파일 정리 |
| [lab02](lab02) | SSU Cleanupd | 디렉토리 모니터링 daemon, 설정 파일, 로그, signal 기반 reload/terminate |
| [lab03](lab03) | SSU-EXT2 | ext2 disk image parser, tree/print command, direct/indirect block traversal |

## Repository Structure

```text
linux-system-programming/
├── lab01/
│   ├── README.md
│   ├── report.pdf
│   └── src/
├── lab02/
│   ├── README.md
│   ├── report.pdf
│   └── src/
├── lab03/
│   ├── README.md
│   ├── report.pdf
│   └── src/
└── README.md
```

각 lab은 C source code, Makefile, 구현 보고서, 세부 README를 포함합니다.

## Environment

- OS: Linux (Ubuntu)
- Language: C
- Compiler: gcc
- Build: Makefile
- Category: Linux System Programming

## Lab Summary

### Lab01 - SSU Cleanup

Linux 환경에서 동작하는 사용자 정의 shell 프로그램을 구현했습니다.

주요 구현:

- `tree <DIR_PATH> [OPTION]`
- `arrange <DIR_PATH> [OPTION]`
- `help`
- `exit`
- `scandir`, `stat`, `lstat` 기반 디렉토리 탐색
- `fork`, `execvp`, `waitpid` 기반 프로세스 제어
- 확장자 기준 파일 분류와 중복 파일 처리
- HOME 디렉토리 외부 접근 제한

### Lab02 - SSU Cleanupd

지정한 디렉토리를 주기적으로 모니터링하고 자동으로 파일을 정리하는 daemon process를 구현했습니다.

주요 구현:

- double-fork 기반 daemon 생성
- `setsid`를 통한 session 분리
- 표준 입출력 `/dev/null` 리다이렉션
- `SIGHUP` 기반 설정 reload
- `SIGTERM` 기반 daemon 종료
- `ssu_cleanupd.config` 설정 관리
- `ssu_cleanupd.log` 로그 관리
- 모니터링 경로 중복/상하위 충돌 검증

### Lab03 - SSU-EXT2

ext2 파일 시스템 이미지 파일을 직접 파싱하는 interactive shell 프로그램을 구현했습니다.

주요 구현:

- ext2 superblock, group descriptor, inode, directory entry 구조체 직접 정의
- magic number, block size, inode size 계산
- inode와 data block을 따라가는 경로 탐색
- `tree <PATH> [OPTION]`
- `print <PATH> [OPTION]`
- direct block, single/double/triple indirect block 처리
- `lseek`, `read` 기반 raw disk image parsing
- ext2 전용 라이브러리 미사용

## What This Repository Shows

이 저장소는 Linux 시스템 프로그래밍에서 자주 등장하는 파일, 프로세스, 시그널, 데몬, 파일 시스템 내부 구조를 단계적으로 구현한 기록입니다.

특히 다음 역량을 보여주는 데 초점을 둡니다.

- POSIX system call 기반 파일 처리
- 디렉토리 순회와 경로 검증
- process 생성/종료/대기 흐름 이해
- daemon process lifecycle 설계
- signal 기반 runtime control
- 설정 파일과 로그 파일 관리
- ext2 on-disk structure parsing
- direct/indirect block 기반 파일 데이터 접근

## Build

각 lab의 `src` 디렉토리에서 빌드합니다.

```bash
cd lab01/src
make
```

```bash
cd lab02/src
make
```

```bash
cd lab03/src
make
```

빌드 결과물 제거:

```bash
make clean
```

## Lab Details

각 lab의 자세한 구현 내용은 개별 README와 report에서 확인할 수 있습니다.

- [LAB01 - SSU Cleanup](lab01/README.md)
- [LAB02 - SSU Cleanupd](lab02/README.md)
- [LAB03 - SSU-EXT2](lab03/README.md)
