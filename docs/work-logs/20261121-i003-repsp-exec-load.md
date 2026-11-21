# 작업 로그: repsp v0.0.30 고정과 첫 런타임 부팅 측정 (issue #3)

* 이슈: [#3](https://github.com/reexec/reDMP2/issues/3) (상류 변경: [reexec/repsp#65](https://github.com/reexec/repsp/issues/65), `v0.0.30`)
* 설계: [20261121-i003-repsp-exec-load.md](../design/20261121-i003-repsp-exec-load.md)
* 작업 지시: [20261121-i003-repsp-exec-load.md](../work-orders/20261121-i003-repsp-exec-load.md)
* 브랜치: `i003-repsp-exec-load`

---

## 1. 변경 요약

1. **서브모듈 고정점 갱신 (`third_party/repsp`)**
   * `7f856eb`(v0.0.29) → `81558ef739530d5fda3cbf40dee0c562b286732e`(v0.0.30 머지 커밋, 고정 주소 `ET_EXEC` 로드 지원).
   * `repsp`의 공개 API 시그니처(`ParsePrx`, `RelocateToBuffer`, `runtime::LoadPrx`) 변경이 없으므로 `reDMP2` 소스 코드는 수정하지 않았습니다. `third_party/psprecomp` 고정점도 유지했습니다.
2. **문서 갱신**
   * [docs/analysis/boot.md](../analysis/boot.md): 정지 위치가 로드 단계(`Unsupported ELF type`)에서 게스트 실행 진입 후 첫 미구현 커널 NID(`SysMemUserForUser` `0x7591C7DB`)로 이동한 결과(**확인됨**), 측정 명령, 요약 수치, CTest 상태(31/32)를 반영했습니다.
   * [docs/EXECUTABLE_DESIGN.ko.md](../EXECUTABLE_DESIGN.ko.md), [docs/EXECUTABLE_DESIGN.en.md](../EXECUTABLE_DESIGN.en.md): `repsp` 기준을 v0.0.30(`81558ef`)으로 갱신하고 첫 런타임 정지 NID를 반영했습니다.

---

## 2. 빌드 및 CTest 검증 (**확인됨**)

### 2.1. 생성 코드 없는 빌드 (CI 조건)
임시 빈 디렉터리(`%TEMP%\redmp2-i003-empty-gen`)를 `-DREDMP2_GENERATED_DIR`로 지정해 `REDMP2_HAVE_RECOMP=0` 구성을 검증했습니다.

```powershell
cmake --preset win64-debug -B build/win64-debug-nogen "-DREDMP2_GENERATED_DIR=$env:TEMP\redmp2-i003-empty-gen"
cmake --build build/win64-debug-nogen --config Debug
ctest --test-dir build/win64-debug-nogen -C Debug --output-on-failure

cmake --preset win64-release -B build/win64-release-nogen "-DREDMP2_GENERATED_DIR=$env:TEMP\redmp2-i003-empty-gen"
cmake --build build/win64-release-nogen --config Release
ctest --test-dir build/win64-release-nogen -C Release --output-on-failure
```

* **Win64 Debug (생성 코드 없음)**: CTest **31/31 통과** (5.62초). 로컬에 `roms/`가 있어 `app_smoke_registered_roms`(식별 전용 모드) 1건이 추가 실행되었으며, `roms/`가 없는 CI 환경 기준 30/30에 해당합니다. `redmp2`/`repsp` 타깃 경고 **0건**(서드파티 `imgui.vcxproj` SDL3 헤더 `C4819` 2건만 기존과 동일).
* **Win64 Release (생성 코드 없음)**: CTest **31/31 통과** (3.76초). `redmp2`/`repsp` 타깃 경고 **0건**(동일 `C4819` 2건).

### 2.2. 생성 코드 포함 Release 빌드 및 CTest
```powershell
cmake --build build/win64-release --config Release
ctest --test-dir build/win64-release -C Release --output-on-failure
```

* **빌드 경고**: **0건**.
* **CTest**: **31/32 통과, 1 실패**
  * `app_lift_check`: **통과** (issue #1의 로드 실패 해소).
  * `app_smoke_registered_roms`: 실패 (아래 첫 미구현 NID 정지 — 설계대로 비활성화하지 않고 유지).

---

## 3. 첫 런타임 헤드리스 부팅 측정 (**확인됨**)

```powershell
.\build\win64-release\Release\redmp2.exe roms --headless --max-vblanks 600 --save-dir "$env:TEMP\redmp2-i003\save" --crash-dir "$env:TEMP\redmp2-i003\crash"
```

* **식별 및 로드**: `ULKS46116` v1.09 `BOOT.BIN`(844,948 B, SHA-1 `1a5c904989d430588da9653acad4b6c5cdab2885`)을 `0x08804000..0x08E2A500`(6,448,384 B)에 로드하고 `$gp = 0x088D9F10`, `entry = 0x0880FC4C`로 `module_start` 스레드(prio 32, stack 256 KiB, `tid = 4`)를 시작했습니다.
* **정지 보고 요약**:
  * **정지 분류**: `(b) Unimplemented kernel NID`
  * **라이브러리 / NID**: `SysMemUserForUser` / `0x7591C7DB` (`sceKernelSetCompiledSdkVersion`)
  * **종료 코드**: `10`
  * **실행 수치**: 디스패치 `0`회, 표시 `0`프레임 (`VBlank = 0`, `GE lists = 0`, `Audio output_calls = 0`)

---

## 4. 다음 HLE 후보 (정적 대조와 첫 정지의 관계)

* issue #1의 정적 임포트 커버리지(`tools/import_coverage.py`)에서 추출한 미구현 NID 88개 중, 런타임 실행 진입 직후 가장 먼저 호출된 항목은 `SysMemUserForUser`의 `0x7591C7DB`(`sceKernelSetCompiledSdkVersion`)로 **확인됨**.
* 같은 `SysMemUserForUser` 미구현 5건(`0x13A5ABEF` `sceKernelPrintf`, `0x7591C7DB` `sceKernelSetCompiledSdkVersion`, `0xA291F107` `sceKernelMaxFreeMemSize`, `0xF77D77CB`, `0xF919F628` `sceKernelTotalFreeMemSize`)과 CRT/초기화 경로에서 자주 호출되는 `ThreadManForUser`(6건), `ModuleMgrForUser`(3건), `IoFileMgrForUser`(13건)가 부팅 초반 후속 정지 후보로 **추정됨**.
* 구체적인 후속 정지 순서는 `repsp`에 `SysMemUserForUser` HLE를 추가한 뒤 재측정하여 확정한다(**미확정**).

---

# Work Log: Pin repsp v0.0.30 and Measure the First Runtime Boot (issue #3)

* Issue: [#3](https://github.com/reexec/reDMP2/issues/3) (upstream: [reexec/repsp#65](https://github.com/reexec/repsp/issues/65), `v0.0.30`)
* Design: [20261121-i003-repsp-exec-load.md](../design/20261121-i003-repsp-exec-load.md)
* Work order: [20261121-i003-repsp-exec-load.md](../work-orders/20261121-i003-repsp-exec-load.md)
* Branch: `i003-repsp-exec-load`

---

## 1. Summary of Changes

1. **Submodule pin (`third_party/repsp`)**: Moved from `7f856eb` (v0.0.29) to `81558ef739530d5fda3cbf40dee0c562b286732e` (v0.0.30 merge commit, fixed-address `ET_EXEC` loading). No `reDMP2` source files or `psprecomp` pin changed.
2. **Documentation**: Updated [docs/analysis/boot.md](../analysis/boot.md), [docs/EXECUTABLE_DESIGN.ko.md](../EXECUTABLE_DESIGN.ko.md), and [docs/EXECUTABLE_DESIGN.en.md](../EXECUTABLE_DESIGN.en.md) to reflect v0.0.30 (`81558ef`) and the new first runtime stop.

## 2. Build and CTest Verification (**confirmed**)

* **Win64 Debug without generated code (`-DREDMP2_GENERATED_DIR=<empty>`)**: **31/31 CTest passed** (5.62 s; includes local `roms/` identification test `app_smoke_registered_roms`, corresponding to 30/30 in CI without `roms/`). 0 warnings on `redmp2`/`repsp` targets (only the 2 pre-existing third-party `imgui.vcxproj` `C4819` warnings on SDL3 headers under CP949).
* **Win64 Release without generated code (`-DREDMP2_GENERATED_DIR=<empty>`)**: **31/31 CTest passed** (3.76 s). 0 warnings on `redmp2`/`repsp` targets (same 2 `C4819` warnings).
* **Win64 Release with generated code**: **0 warnings**, **31/32 CTest passed** (`app_lift_check` now passes; `app_smoke_registered_roms` fails on the first unimplemented NID stop and is kept enabled).

## 3. First Runtime Headless Boot (**confirmed**)

Ran `redmp2.exe roms --headless --max-vblanks 600 --save-dir <temp>\save --crash-dir <temp>\crash`:
* Loaded `ULKS46116` v1.09 `BOOT.BIN` at `0x08804000..0x08E2A500` (`$gp = 0x088D9F10`, `entry = 0x0880FC4C`) and started `module_start` (prio 32, 256 KiB stack, `tid = 4`).
* **Stop report summary**: class `(b) Unimplemented kernel NID`, `SysMemUserForUser` `0x7591C7DB` (`sceKernelSetCompiledSdkVersion`), exit code `10`, `0` dispatches, `0` frames presented (`VBlank = 0`, `GE lists = 0`, `Audio output_calls = 0`).

## 4. Next HLE Candidates

* **Confirmed**: Of the 88 missing NIDs from issue #1's static coverage, `SysMemUserForUser` `0x7591C7DB` (`sceKernelSetCompiledSdkVersion`) is the first invoked at runtime.
* **Inferred**: Remaining `SysMemUserForUser` NIDs (5 total), along with early CRT/startup `ThreadManForUser` (6), `ModuleMgrForUser` (3), and `IoFileMgrForUser` (13) NIDs, are the next likely stops along the boot path.
* **Unresolved**: The exact sequence of subsequent runtime stops will be measured once `SysMemUserForUser` HLE support is added in `repsp`.
