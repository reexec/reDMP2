# 작업 로그: 프로젝트 부트스트랩 — reDMP1 구조 이식, ULKS-46116 신원, lift·link·첫 부팅 측정 (issue #1)

* 설계: [docs/design/20261121-i001-project-bootstrap.md](../design/20261121-i001-project-bootstrap.md)
* 작업 지시: [docs/work-orders/20261121-i001-project-bootstrap.md](../work-orders/20261121-i001-project-bootstrap.md)
* 브랜치: `i001-project-bootstrap`

---

## 1. 단계 A: 저장소 골격과 생성 코드 없는 빌드

### 수행 내용 및 커밋
* `2314ebf` (`Add repsp and psprecomp submodules at pinned commits (#1)`):
  * `third_party/repsp` (`7f856eb`, v0.0.29; 하위 `SDL` `ee239da4`, `imgui` `c1aaac65` 포함)와 `third_party/psprecomp` (`5ca18fa`)를 추가하고 재귀 초기화.
* `cea89b4` (`Port repository skeleton, docs, ABI adapter, and app without autoplay (#1)`):
  * `E:\MYWORK\Projects\reexec\reDMP1` (`dadb6da`, v0.0.30)에서 루트 파일, 규칙 문서, 가이드, `src/recomp_abi/`, `src/game/dmp2/`, `include/redmp2/`, `src/app/`(autoplay 완전 제외), `tests/`, `scripts/`, `tools/`, `patches/psprecomp/`, `.github/workflows/`를 치환 규칙에 따라 이식.
  * `VERSION`은 `0.0.0`, `seed_functions.json`과 초기 `variants.json`은 `[]`로 시작.
  * `src/recomp_abi/trace.c`에서 DMP1 전용 refcount/`__assert` 주소 분기 제거.
* `86eeac7` (`Pass REDMP2_LIFT_BASE as a build-time compile definition (#1)` — 단계 A 리뷰 수정):
  * `CMakeLists.txt`에서 `lift.json`의 `base`를 읽어 `REDMP2_LIFT_BASE=${REDMP2_LIFT_BASE}u` 컴파일 정의로 전달하고, `src/app/main.cpp`의 런타임 `LoadLiftBase()` 경로 탐색을 제거.

### 발견 사항 (`ET_EXEC` vs PRX) 및 빌드·CTest 결과
* 최초 CTest 실행 시 `reloc_check`가 `repsp::module::RelocateToBuffer failed: Unsupported ELF type (expected PRX e_type 0xFFA0)`로 실패했고, `lift_invariance`가 `ET_EXEC`에 `--base 0x08804000`을 전달하면서 `allegrexrecomp`가 정지(hang)하는 현상을 확인했다.
* DMP2 `BOOT.BIN`이 `e_type = 2`(`ET_EXEC`)임을 확인하고, `CMakeLists.txt`에서 PRX 재배치 전용 테스트(`reloc_check*`, `lift_invariance`)가 `e_type == 0xFFA0`일 때만 등록되도록 가드했다.
* **빌드/CTest (생성 코드 없음)**:
  * `cmake --preset win64-debug && cmake --build build/win64-debug --config Debug && ctest --test-dir build/win64-debug -C Debug --output-on-failure`: **reDMP2 타깃 경고 0건**, CTest **30/30 통과** (5.29초).
  * `cmake --preset win64-release && cmake --build build/win64-release --config Release && ctest --test-dir build/win64-release -C Release --output-on-failure`: **reDMP2 타깃 경고 0건**, CTest **30/30 통과** (3.79초).
  * 서드파티 참고: 한글 로케일(CP949) Windows에서 `third_party/repsp`의 `imgui.vcxproj`가 SDL3 헤더(`SDL_video.h`, `SDL_timer.h`)에 대해 `warning C4819` 2건을 출력했다(서브모듈 무수정 원칙에 따라 유지).

---

## 2. 단계 B: 디스크 식별, lift, link

### 수행 내용 및 커밋
* `dbfaa36` (`Handle ET_EXEC in regenerate.py and register ULKS46116 variant (#1)`):
  * `scripts/recomp/lifter.py`에 기본 `120s` 타임아웃을 추가해 `allegrexrecomp` 정지를 방지.
  * `scripts/recomp/regenerate.py`가 입력 ELF의 `e_type`을 검사해 `0xFFA0`(PRX)일 때만 `--base`를 전달하고, `2`(`ET_EXEC`)일 때는 `--base`를 생략하되 첫 `PT_LOAD` `p_vaddr == lift.json base`를 검증하도록 수정.
  * `python scripts/recomp/register_disc.py roms` 실행 → `src/game/dmp2/recomp/variants.json`에 `ULKS46116` v1.09 (`BOOT.BIN`, 844,948 B, SHA-1 `1a5c904989d430588da9653acad4b6c5cdab2885`) 등록.

### 측정 수치
* `.\build\win64-release\Release\redmp2.exe roms` (생성 코드 없는 빌드): `ULKS46116` v1.09 정상 식별(종료 코드 0).
* `python scripts/recomp/regenerate.py roms` (`--trampoline`, `ET_EXEC` `--base` 생략):
  * `7,933` 함수, `27,936` 내부 진입점, `.text` 커버리지 **97.5%** (`753,592 / 772,856` B), 도달 고유 임포트 `208`개 (`229`개 / `27`개 라이브러리), 미해결 간접 점프 `0`, 점프 테이블 `355`개(`4,560` 타깃), VFPU 트랩 `84`개(`0.04%`), invalid `37`개(`0.02%`), 총 `127`개 생성 파일.
* `python tools/lift_audit.py --generated src/game/dmp2/recomp/generated --base 0x08804000`:
  * `7,933` 본문 감사 (`35,869` thunks), `data_body: 0`, `mismatches: 12` (전부 본문 끝 `j -> psp_import_*()` 꼬리 호출), tail-call SCC `284`개.
* **생성 코드 포함 빌드·CTest**:
  * **Release**: `redmp2_recomp.lib` + `redmp2.exe` 링크 성공, **경고 0건**, CTest **30/32 통과, 2 실패** (3.71초).
  * **Debug**: `redmp2_recomp.lib` + `redmp2.exe` 링크 성공, **경고 0건**, CTest **30/32 통과, 2 실패** (5.12초).
  * 실패 2건(`app_smoke_registered_roms`, `app_lift_check`, 로컬 전용)은 모두 `LoadPrx failed: Unsupported ELF type (expected PRX e_type 0xFFA0)`로 인한 것으로, 비활성화하지 않고 유지했다.

---

## 3. 단계 C: 첫 부팅 측정, 정적 임포트 커버리지 및 문서

### 수행 내용
* **C1 (첫 부팅 측정)**:
  * `.\build\win64-release\Release\redmp2.exe roms --headless --max-vblanks 600` 및 창 모드 `.\build\win64-release\Release\redmp2.exe roms` 실행.
  * 두 실행 모두 `ULKS46116` v1.09 식별 직후 `repsp::runtime::LoadPrx`에서 `Unsupported ELF type (expected PRX e_type 0xFFA0)`로 종료(코드 `1`). 첫 화면 없음, 게스트 코드 진입 전 정지.
* **C1a (`tools/import_coverage.py`)**:
  * Python 표준 라이브러리만으로 `.lib.stub`와 `third_party/repsp/src/hle/`의 `Register`/`RegisterByName` 등록 목록을 정적 대조하는 도구 작성.
  * `python -I tools/import_coverage.py roms/PSP_GAME/SYSDIR/BOOT.BIN` 결과: **27개 라이브러리, 229개 고유 NID 중 141개 구현(61.6%), 88개 미구현(38.4%)**.
* **C2~C4 (문서 및 검증)**:
  * 작성/갱신: `docs/analysis/disc-image.md`, `docs/analysis/executable.md`, `docs/analysis/boot.md`, `docs/analysis/README.md`, `docs/EXECUTABLE_DESIGN.ko.md`, `docs/EXECUTABLE_DESIGN.en.md`, 본 작업 로그.
  * `git grep -i -E "dmp1|redmp1"` 검사에서 출처 표기와 reDMP1 비교 설명 외의 잔여 참조가 없음을 확인.

---

## 4. 남은 문제 및 후속 issue 제안

1. **(`repsp` issue) `ET_EXEC` (`e_type = 2`) 모듈 로드 지원**:
   * 현재 `repsp::module::ProcessPrx`가 `e_type == 0xFFA0`만 허용해 DMP2 `BOOT.BIN` 로드 단계에서 정지한다. `ET_EXEC` 세그먼트 적재 및 `.rodata.sceModuleInfo`·`.lib.stub` 파싱을 `repsp`에 추가하고 서브모듈을 올리면 `app_smoke_registered_roms`·`app_lift_check` 및 첫 런타임 부팅 측정이 가능해진다.
2. **(`repsp` issue) C1a에서 확인된 미구현 HLE 임포트 보강**:
   * 우선순위: `sceUtility` 모듈 로드 함수(미구현 9건 중 AV/Net 모듈 로드 추정), `sceNet`/`sceNetAdhoc`/`sceNetAdhocctl`/`sceNetAdhocMatching`(0/23 — 애드혹 미지원 반환 최소 HLE), `sceHprm`(0/2), `sceImpose`(0/2), `sceRtc`(`sceRtcGetCurrentClockLocalTime`, 0/1), 그리고 실제 부팅 시 호출되는 `IoFileMgrForUser`/`SysMemUserForUser`/`ThreadManForUser`/`sceSasCore` 누락 NID.
3. **(`reDMP2` issue) DMP2 전용 Autoplay (분석 후)**:
   * DMP1 메모리 구조에 묶여 있던 `autoplay`는 이번 부트스트랩에서 제외했다. DMP2 부팅 및 채보/입력 메모리 구조 분석이 완료된 뒤 별도 issue로 설계·구현한다.

---

# Work Log: Project Bootstrap — Porting reDMP1 Structure, ULKS-46116 Identity, Lift, Link, and First Boot Measurement (issue #1)

## 1. Phase A: Repository Skeleton and Build Without Generated Code

* Commits: `2314ebf` (submodules `third_party/repsp` @ `7f856eb`, `third_party/psprecomp` @ `5ca18fa`), `cea89b4` (ported structure, docs, ABI adapter, app without autoplay, tests, scripts, tools, patches, workflows), `86eeac7` (review fix passing `REDMP2_LIFT_BASE` as a build-time compile definition and removing runtime `LoadLiftBase()`).
* Finding: `BOOT.BIN` is `ET_EXEC` (`e_type = 2`), not a relocatable PRX (`0xFFA0`), which caused `reloc_check` to fail and `lift_invariance` (`--base 0x08804000`) to hang. Guarded PRX-relocation tests in `CMakeLists.txt` on `e_type == 0xFFA0`.
* Verification (without generated code): Win64 Debug and Release both built with **0 reDMP2 target warnings** (2 third-party `C4819` warnings in `repsp/imgui.vcxproj` under CP949 locale) and **30/30 CTest passed**.

## 2. Phase B: Disc Identity, Lift, and Link

* Commit: `dbfaa36` (added 120s timeout to `lifter.py`, updated `regenerate.py` to omit `--base` and verify `p_vaddr == lift.json base` for `ET_EXEC`, registered `ULKS46116` v1.09 in `variants.json`).
* Lift results: `7,933` functions, `27,936` interior entries, **97.5%** `.text` coverage (`753,592 / 772,856` bytes), `208` distinct reached imports (`229` declared across `27` libraries), `0` unresolved indirect jumps (`355` jump tables), `84` VFPU traps (`0.04%`), `37` invalid words (`0.02%`). `tools/lift_audit.py`: `0` data bodies, `12` `j` tail-import mismatches, `284` tail-call SCCs.
* Build & CTest (with generated code): Win64 Release and Debug both compiled and linked with **0 warnings**; CTest **30/32 passed, 2 failed** (`app_smoke_registered_roms`, `app_lift_check` — both failing at `LoadPrx` rejecting `ET_EXEC`, kept enabled as expected).

## 3. Phase C: First Boot Measurement, Static Import Coverage, and Documentation

* C1: Both headless (`redmp2.exe roms --headless --max-vblanks 600`) and windowed (`redmp2.exe roms`) runs stop before guest execution with exit code `1` (`LoadPrx failed: Unsupported ELF type (expected PRX e_type 0xFFA0)`).
* C1a: Added `tools/import_coverage.py` (stdlib-only, `python -I`). Static comparison against `third_party/repsp` (`7f856eb`, 146 registered NIDs) reports **141/229 (61.6%) implemented** and **88/229 (38.4%) missing** across 27 libraries.
* C2–C4: Created `docs/analysis/{disc-image,executable,boot}.md`, updated `docs/analysis/README.md`, created `docs/EXECUTABLE_DESIGN.{ko,en}.md` and this work log, and verified `git grep -i -E "dmp1|redmp1"`.

## 4. Proposed Follow-Up Issues

1. **`repsp` issue — Support loading `ET_EXEC` (`e_type = 2`) modules**: unblock `LoadPrx`, `app_smoke_registered_roms`, `app_lift_check`, and runtime boot tracing.
2. **`repsp` issue — HLE for missing DMP2 imports found in C1a**: `sceUtility` module loading, minimal unavailable-ad-hoc HLE for `sceNet*`/`sceNetAdhoc*` (23 NIDs), `sceHprm` (2 NIDs), `sceImpose` (2 NIDs), `sceRtc` (1 NID), plus runtime-reached kernel/SAS NIDs.
3. **`reDMP2` issue — DMP2 autoplay**: design and implement after DMP2 chart/input memory analysis.
