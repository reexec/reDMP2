# 설계: 디스크 ID별 `roms/` 구조 (issue #21)

## 1. 요구사항

* 사용자 결정(2026-11-24): `roms/`를 디스크 ID별로 나눈다. 링크 디스크는 UMD 교체 방식으로 구현한다(별도 issue).
* 빌드와 실행에는 DMP2만 필요하다. 다른 디스크가 없거나 그 폴더가 비어 있어도 링크 디스크 말고는 모두 지금처럼 동작해야 한다.

## 2. 구조

```
roms/
├─ ULKS46116/          DMP2 (필수)
│  ├─ PSP_GAME/
│  └─ UMD_DATA.BIN
└─ ULKS46050/          DJMAX Portable 1 (선택, 링크 디스크)
   └─ PSP_GAME/ ...
```

* 리뷰어가 로컬 덤프를 이미 이 구조로 옮겼다(`roms/ULKS46116/`, 빈 `roms/ULKS46050/`).
* `roms/`는 계속 git이 무시한다.
* 폴더 이름은 관례일 뿐이다. 디스크 ID는 항상 그 폴더의 `PSP_GAME/PARAM.SFO`에서 읽는다.

## 3. 디스크 고르기 (앱과 스크립트가 같은 규칙)

입력 경로 `P`를 다음처럼 해석한다.

1. `P/PSP_GAME/PARAM.SFO`가 있으면 `P`가 디스크 폴더다. 예전 구조(`roms/PSP_GAME`)도 이렇게 계속 동작한다.
2. 없으면 `P`를 디스크 모음 폴더로 본다.
   * 바로 아래 폴더 중 `PSP_GAME/PARAM.SFO`가 있는 것을 훑는다.
   * `variants.json`에 등록된 디스크(DMP2)를 주 디스크로 고른다.
   * 등록된 디스크가 없거나 둘 이상이면 오류로 끝낸다. 메시지에 찾은 디스크 ID 목록과 기대 위치를 적는다.
3. 주 디스크 말고 찾은 디스크(링크 디스크 후보)는 ID와 경로만 기록한다(로그 한 줄). 쓰는 곳은 링크 디스크 issue다.
   * 빈 폴더, `PARAM.SFO`가 없는 폴더, 읽을 수 없는 폴더는 조용히 건너뛴다.
   * 그런 폴더 때문에 실패하지 않는다.

* **앱**: `redmp2.exe roms`와 `redmp2.exe roms/ULKS46116`이 둘 다 동작한다. 기본 `variants.json` 찾기(지금은 `game_dir/..` 기준)가 두 경우 모두 맞도록 고친다.
* **스크립트·도구**: `scripts/recomp/regenerate.py`, `scripts/recomp/register_disc.py`, `tools/import_coverage.py`의 기본 경로가 새 구조에서 DMP2를 찾게 한다. 공용 Python 함수 하나에 위 규칙을 두고 함께 쓴다.
* **CMake 테스트**: `roms/` 경로를 쓰는 검사(`app_smoke_registered_roms`, 재배치 검사, 진입점 검사)가 새 구조의 DMP2를 쓴다. DMP2가 없으면 지금처럼 그 테스트들을 건너뛴다.

## 4. 문서

* 사용자 안내에 새 구조와 실행 예를 적는다: `README.md`, `docs/guides/*`, `scripts/build_win64.bat`의 실행 안내.
* `AGENTS.md`·`ARCHITECTURE.md`에 `roms/` 설명이 있으면 맞춘다.
* 지난 설계·작업 로그·릴리스 노트는 당시 기록이므로 고치지 않는다.

## 5. 검증

1. 새 구조에서 다음이 모두 지금과 같다:
   * `regenerate.py`: 함수 7,982, 진입점 28,109
   * `import_coverage.py`: 163/229
   * `lift_audit` 0/0
2. CTest: 생성 코드 포함 Release 34/34(DMP2 테스트 포함), 생성 코드 없이 Debug/Release. Linux는 CI.
3. 실행 확인: 아래 세 경우 모두 DMP2로 부팅한다(headless 짧은 실행).
   * `redmp2.exe roms`
   * `redmp2.exe roms/ULKS46116`
   * 예전 구조를 흉내 낸 임시 폴더(`<tmp>/PSP_GAME`)
4. 실패 확인: 주 디스크가 없는 모음 폴더는 분명한 오류로 끝난다.
5. `roms/ULKS46050/`가 비어 있을 때와 다른 디스크가 들어 있을 때 모두 DMP2 동작이 같다.

---

# Design: `roms/` per disc ID (issue #21)

## 1. Requirements

* User decision (2026-11-24): split `roms/` per disc ID. Link disc is implemented as a UMD swap (separate issue).
* Build and run need only DMP2. With other discs missing or their folders empty, everything except link disc works as now.

## 2. Layout

The tree is shown above: `roms/ULKS46116/` holds DMP2 (required) and `roms/ULKS46050/` holds DJMAX Portable 1 (optional, link disc).

* The reviewer has already moved the local dump into this layout (`roms/ULKS46116/`, and an empty `roms/ULKS46050/`).
* `roms/` stays ignored by git.
* Folder names are only a convention. The disc ID is always read from that folder's `PSP_GAME/PARAM.SFO`.

## 3. Choosing discs (one rule for the app and the scripts)

An input path `P` is resolved as follows:

1. If `P/PSP_GAME/PARAM.SFO` exists, `P` is a disc folder. The old layout (`roms/PSP_GAME`) keeps working this way.
2. Otherwise `P` is a folder of discs.
   * Scan its immediate subfolders that contain `PSP_GAME/PARAM.SFO`.
   * Choose the disc registered in `variants.json` (DMP2) as the main disc.
   * If no registered disc is found, or more than one, exit with an error that lists the disc IDs found and the expected location.
3. Other discs found (link disc candidates) are recorded by ID and path only, in one log line. The link disc issue uses them.
   * Empty folders, folders without `PARAM.SFO`, and unreadable folders are skipped quietly.
   * None of them causes a failure.

* **App**: both `redmp2.exe roms` and `redmp2.exe roms/ULKS46116` work. The default `variants.json` lookup (today relative to `game_dir/..`) is fixed for both cases.
* **Scripts and tools**: the default paths of `scripts/recomp/regenerate.py`, `scripts/recomp/register_disc.py` and `tools/import_coverage.py` find DMP2 in the new layout. The rule above lives in one shared Python function.
* **CMake tests**: checks that use the `roms/` path (`app_smoke_registered_roms`, the relocation check, the entry check) use DMP2 in the new layout. Without DMP2 they are skipped, as now.

## 4. Documents

* User-facing instructions get the new layout and run examples: `README.md`, `docs/guides/*`, and the run hint in `scripts/build_win64.bat`.
* `AGENTS.md` and `ARCHITECTURE.md` are updated if they describe `roms/`.
* Past designs, work logs and release notes are records of their time and are not changed.

## 5. Verification

1. In the new layout these all match today's values:
   * `regenerate.py`: 7,982 functions, 28,109 entries
   * `import_coverage.py`: 163/229
   * `lift_audit` 0/0
2. CTest: Release with generated code 34/34 (including the DMP2 tests), and Debug/Release without generated code. Linux in CI.
3. Runs: all three cases boot DMP2 (short headless run).
   * `redmp2.exe roms`
   * `redmp2.exe roms/ULKS46116`
   * a temporary folder mimicking the old layout (`<tmp>/PSP_GAME`)
4. Failure: a folder of discs without the main disc ends with a clear error.
5. DMP2 behaves the same whether `roms/ULKS46050/` is empty or holds another disc.
