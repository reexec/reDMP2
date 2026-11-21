# 작업 지시: 프로젝트 부트스트랩 (issue #1)

설계: [20261121-i001-project-bootstrap.md](../design/20261121-i001-project-bootstrap.md). 이식 원본: 로컬 `E:\MYWORK\Projects\reexec\reDMP1`(`main`, `dadb6da`, v0.0.30). 브랜치: `i001-project-bootstrap`. 커밋 제목 끝에 `(#1)`.

치환 규칙(모든 단계 공통): `reDMP1`→`reDMP2`, `redmp1`→`redmp2`, `REDMP1`→`REDMP2`, `dmp1`→`dmp2`, `DMP1`→`DMP2`, `DJMAX Portable`/`DJMAX PORTABLE`(타이틀 이름)→`DJ Max Portable 2`, `ULKS-46050`/`ULKS46050`→`ULKS-46116`/`ULKS46116`, 디스크 버전 1.02→1.09, redump #33114→#33115. 출처 표기("reDMP1에서 가져옴")와 reDMP1과의 비교 설명은 예외로 남긴다.

## 단계 A: 저장소 골격과 생성 코드 없는 빌드

1. submodule 추가: `third_party/repsp`(`https://github.com/reexec/repsp`, `7f856eb`), `third_party/psprecomp`(`https://github.com/sp00nznet/psprecomp`, `5ca18fa`). `git submodule update --init --recursive`로 repsp의 하위 submodule까지 받는다.
2. 루트 파일 이식: `.gitattributes`, `.gitignore`(ISO와 `roms/`, `redmp2.ini`, `src/game/dmp2/recomp/generated/` 포함), `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`, `LICENSE`, `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `VERSION`(`0.0.0`).
3. `README.md`, `ARCHITECTURE.md`, `CREDITS.md`, `THIRD_PARTY_NOTICES.md`: reDMP1 것을 바탕으로 reDMP2의 현재 상태(autoplay 없음, reDMP1에서 이식했다는 사실, ULKS-46116)로 다시 쓴다. 한국어 먼저, 영어 번역이 바로 아래.
4. `docs/CODING_STYLE.md`, `docs/PROJECT_CHARTER.md`, `docs/guides/`(autoplay처럼 해당 없는 절차는 뺌), `docs/analysis/README.md`, `docs/kb/README.md`.
5. 코드 이식: `src/recomp_abi/`, `src/game/dmp2/`(profile, `decomp/README.md`, `recomp/CMakeLists.txt`, `recomp/lift.json`의 `prefix`는 `dmp2`, `recomp/seed_functions.json`은 `[]`, `recomp/variants.json`은 단계 B에서 생성하므로 이 단계에서는 `[]`), `include/redmp2/`, `src/app/`(**autoplay 제외**: `autoplay.cpp/.h/_probe.cpp`를 가져오지 않고 main.cpp의 `--autoplay`, 버튼 필터 결합, settings UI의 Autoplay 체크박스·배지·생성자 인자를 제거한다), `tests/`, `scripts/`, `tools/`, `patches/psprecomp/`, `.github/workflows/`.
6. 코드 안에 DMP1 고유 값(예: `0x08804000` 하드코딩, DMP1 주소)이 있으면 `lift.json`에서 읽게 하거나, 그대로 둘 때는 작업 로그에 위치와 이유를 적는다.
7. 검증: `scripts/build_win64.ps1`(또는 reDMP1과 같은 프리셋)로 Debug와 Release를 생성 코드 없이 빌드하고 CTest 전체 통과. 경고 0.
8. `git status`에 게임 데이터가 없는지 확인하고 커밋한다.

## 단계 B: 디스크 식별, lift, link

1. `python scripts/recomp/register_disc.py roms`로 `variants.json`을 만든다. 기대값: `ULKS46116`, `1.09`, `BOOT.BIN`, SHA-1 `1a5c904989d430588da9653acad4b6c5cdab2885`, 크기 844,948.
2. 앱이 `roms`를 식별하는지 확인한다(`redmp2 --lift-check roms` 또는 식별 출력).
3. `python scripts/recomp/regenerate.py`로 `generated/`를 만든다. 함수 수, 내부 진입점 수, `.text` 커버리지, 경고를 기록한다. `tools/lift_audit.py`도 실행해 결과를 기록한다.
4. 생성 코드를 포함해 Release(필요하면 Debug도) 빌드, 링크 성공, CTest 통과.
5. 커밋(`variants.json`, 문서. 생성 코드는 커밋하지 않음).

## 단계 C: 첫 부팅 측정과 문서

1. `redmp2 --headless --max-vblanks 600 roms`와 창 모드 `redmp2 roms`(최대 1분)를 실행한다. 정지 분류, 마지막 로그 80줄, 미구현 import 목록, 첫 화면 여부를 기록한다. 로그 파일은 커밋하지 않는다. (단계 B 결과: 실행은 repsp `LoadPrx`의 `ET_EXEC` 거부에서 멈춘다. 이 사실과 CTest 실패 2건(`app_smoke_registered_roms`, `app_lift_check`, 로컬 전용)을 그대로 기록한다.)
1a. 로드에서 멈추므로 실행 없이 얻을 수 있는 것을 정적으로 측정한다: `tools/import_coverage.py`(새 도구)로 lift 입력의 import NID(라이브러리, NID, 가능하면 공개 이름)를 repsp가 등록하는 HLE NID와 대조해, 라이브러리별 구현/미구현 수와 미구현 NID 목록을 낸다. repsp의 등록 목록은 repsp 소스(`third_party/repsp/src/hle/`)에서 읽는다(repsp는 고치지 않는다).
2. 문서: `docs/analysis/disc-image.md`, `docs/analysis/executable.md`, `docs/analysis/boot.md`(확인됨/추정/미확정 구분), `docs/analysis/README.md` 색인, `docs/EXECUTABLE_DESIGN.ko.md`/`.en.md`.
3. 작업 로그 `docs/work-logs/20261121-i001-project-bootstrap.md`: 단계별 수행 내용, 명령, 결과 수치, 남은 문제와 후속 issue 제안.
4. 설계 §6의 검증 7(`git grep -i -E "dmp1|redmp1"`)을 확인하고 커밋한다.

## 금지

* push, PR, 태그, issue 생성은 하지 않는다(리드가 한다).
* repsp·psprecomp submodule 안의 파일을 고치지 않는다. 필요하면 멈추고 보고한다.
* PPSSPP·FFmpeg 코드를 가져오지 않는다.

---

# Work order: project bootstrap (issue #1)

Design: [20261121-i001-project-bootstrap.md](../design/20261121-i001-project-bootstrap.md). Source of the port: local `E:\MYWORK\Projects\reexec\reDMP1` (`main`, `dadb6da`, v0.0.30). Branch: `i001-project-bootstrap`. Commit titles end with `(#1)`.

Renaming rules (all phases): `reDMP1`→`reDMP2`, `redmp1`→`redmp2`, `REDMP1`→`REDMP2`, `dmp1`→`dmp2`, `DMP1`→`DMP2`, the title name `DJMAX Portable`/`DJMAX PORTABLE`→`DJ Max Portable 2`, `ULKS-46050`/`ULKS46050`→`ULKS-46116`/`ULKS46116`, disc version 1.02→1.09, redump #33114→#33115. Attributions ("taken from reDMP1") and comparisons with reDMP1 stay.

## Phase A: repository skeleton and a build without generated code

1. Add submodules `third_party/repsp` (`7f856eb`) and `third_party/psprecomp` (`5ca18fa`), initialized recursively.
2. Carry over the root files (`.gitignore` covering the ISO, `roms/`, `redmp2.ini` and `src/game/dmp2/recomp/generated/`), `VERSION` `0.0.0`.
3. Rewrite `README.md`, `ARCHITECTURE.md`, `CREDITS.md`, `THIRD_PARTY_NOTICES.md` for reDMP2's current state, Korean first with the English translation right below.
4. `docs/CODING_STYLE.md`, `docs/PROJECT_CHARTER.md`, `docs/guides/` (without procedures that do not apply), the `docs/analysis/` and `docs/kb/` indexes.
5. Port the code: `src/recomp_abi/`, `src/game/dmp2/` (`lift.json` prefix `dmp2`, `seed_functions.json` `[]`, `variants.json` `[]` until phase B), `include/redmp2/`, `src/app/` **without autoplay** (no `autoplay.*`; remove `--autoplay`, the button-filter hook, and the settings UI's Autoplay checkbox, badge and constructor argument), `tests/`, `scripts/`, `tools/`, `patches/psprecomp/`, `.github/workflows/`.
6. DMP1-specific values in code (a hard-coded `0x08804000`, DMP1 addresses) are read from `lift.json`, or, if left, recorded in the work log with their place and reason.
7. Verify: Debug and Release build without generated code, all CTest tests pass, zero warnings.
8. Check `git status` for game data and commit.

## Phase B: disc identity, lift, link

1. `register_disc.py roms` writes `variants.json` (expected `ULKS46116`, `1.09`, `BOOT.BIN`, SHA-1 `1a5c904989d430588da9653acad4b6c5cdab2885`, size 844,948).
2. The app identifies `roms`.
3. `regenerate.py` produces `generated/`; record function count, internal entries, `.text` coverage and warnings, and run `tools/lift_audit.py`.
4. Build with the generated code, link, CTest passes.
5. Commit (`variants.json`, documents; never generated code).

## Phase C: first boot measurement and documents

1. Run `redmp2 --headless --max-vblanks 600 roms` and windowed `redmp2 roms` (at most a minute). Record the stop class, the last 80 log lines, unimplemented imports and whether a first frame appears. Logs are not committed. (Phase B result: the run stops at repsp `LoadPrx` rejecting `ET_EXEC`; record that and the two failing local-only CTest tests, `app_smoke_registered_roms` and `app_lift_check`, as they are.)
1a. Since the run stops at load, measure statically what does not need a run: a new `tools/import_coverage.py` matches the lift input's import NIDs (library, NID, public name where known) against the HLE NIDs repsp registers, reporting implemented/missing counts per library and the missing NIDs. repsp's registrations are read from its source (`third_party/repsp/src/hle/`), which is not modified.
2. Documents: `docs/analysis/disc-image.md`, `executable.md`, `boot.md` (confirmed/inferred/unresolved), the analysis index, `docs/EXECUTABLE_DESIGN.ko.md`/`.en.md`.
3. Work log `docs/work-logs/20261121-i001-project-bootstrap.md`: what was done per phase, commands, measured numbers, open problems and proposed follow-up issues.
4. Check design §6 item 7 and commit.

## Not allowed

* No push, pull request, tag or issue (the lead does those).
* No edits inside the repsp or psprecomp submodules; stop and report if one is needed.
* No PPSSPP or FFmpeg code.
