# 실행 및 HLE 추적 가이드

이 문서는 사용자가 `redmp2`를 직접 실행해 커널·UMD HLE 부팅 상태와 NID 호출 추적을 확인하는 절차를 설명한다.

---

## 1. 기본 부팅 실행

1. Win64 Release 빌드 후 게임 디렉터리(`roms/`)를 인자로 전달해 실행한다. 창 없이 실행하려면 `--headless`를 붙인다(오디오는 더미 풀 클록으로 구동). 제한된 VBlank 수만큼만 실행하려면 `--max-vblanks <N>`을 지정하며, 정해진 VBlank 시점에 컨트롤러 입력을 주입하려면 `--input-script <파일>`을 사용한다.
   * `--renderer <sw|gl>`: 렌더 백엔드를 선택한다(창 모드 기본값 `gl`, `--headless` 기본값 `sw`). `gl`은 OpenGL 3.3 Core 백엔드를 사용하며, GL 문맥 생성에 실패하면 1줄 경고를 남기고 `sw`로 폴백한다. `--headless --renderer gl`은 숨김 창으로 GL 문맥을 만들어 실행한다.
   * `--scale <1..8>`: 창 배율(기본값 `2`)을 지정한다. `--renderer gl`에서는 내부 FBO 해상도(`480*scale × 272*scale`)에도 동일하게 적용된다.
   * `--audio-latency-ms <N>`: 호스트 측 오디오 중간 버퍼의 목표 지연(ms, 기본값 `20`)을 지정한다.
   * `--save-dir <디렉터리>`: `ms0:/PSP/SAVEDATA`에 연결할 호스트 세이브 디렉터리를 지정한다. 생략하면 OS 사용자 데이터 디렉터리(`SDL_GetPrefPath` 기반) 아래 `savedata/`를 사용한다.
   * `--dialog-answer <yes|no>`: 그리지 않는 Yes/No 메시지 다이얼로그의 응답을 지정한다(기본값 `yes`).
   * `--crash-dir <디렉터리>`, `--no-input-record`, `--debug-crash <kind>`: 실행별 로그·입력 녹화·크래시 보고서/미니덤프 저장 위치와 테스트 크래시를 제어한다(자세한 분석·재현 절차는 [crash-reporting.md](crash-reporting.md) 참고).
   ```powershell
   .\build\win64-release\Release\redmp2.exe roms --variants src\game\dmp2\recomp\variants.json --headless --max-vblanks 600
   ```
2. 종료 후 출력되는 `[boot] === BOOT STOP REPORT ===`와 종료 코드를 확인한다.
   * **분류 (a) (종료 코드 `0`)**: 비커널 NID 미구현 지점 도달 또는 정상 종료(`sceKernelExitGame` 포함).
   * **분류 (b) (종료 코드 `10` 또는 `11`)**: 커널 NID 미구현 또는 예기치 않은 런타임/HLE 정지.
   * **분류 (c) (종료 코드 `12`)**: CPU 폴트 또는 호스트 예외.
   * **분류 (e) (종료 코드 `0`)**: `--max-vblanks <N>` 상한 도달 정지.
   * 정지 보고서 하단에는 `Renderer:`, `OpenGL:`(`gl` 활성 시), `Present:`, `Display:`, `Ctrl:`, `GE:`, `Audio:`, `SAS:`, `ATRAC decode:`, `AVC decode:`, `Utility:`, `Dialog[i]:`, `Scheduler:`(`REPSP_SCHED_STATS=1` 시) 통계 줄이 출력된다.
3. 프레임버퍼가 latch된 프레임을 BMP로 저장하려면 `REPSP_FRAME_DUMP=<디렉터리>`와 `REPSP_FRAME_DUMP_LIMIT=<N>`을 설정한다(`--renderer gl`에서도 배율 1로 readback해 저장). 두 BMP의 픽셀 차이를 비교하려면 `python tools/compare_bmp.py <a.bmp> <b.bmp> [--tol 1] [--diff-out diff.bmp]`를 사용한다.

### 기본 컨트롤러 입력 매핑 (`sceCtrl`)

GUI 실행 시 SDL3 UI 스레드가 키보드·첫 번째 게임패드 폴링과 SDL3 오디오 스트림 출력을 담당하며, `--headless` 실행 시에는 중립 컨트롤러 상태(버튼 `0`, 아날로그 `128, 128`)와 더미 오디오 클록만 공급한다.

| PSP 입력 | 키보드 | 게임패드 (SDL3 1번 패드) |
| --- | --- | --- |
| D-Pad (Up / Right / Down / Left) | 방향키 | D-Pad |
| Cross / Circle / Square / Triangle | `Z` / `X` / `A` / `S` | South / East / West / North |
| L / R Trigger | `Q` / `W` | Left / Right Shoulder |
| Start / Select | `Enter` / `Backspace` | Start / Back |
| Analog Stick (`lx`, `ly`) | `I` / `J` / `K` / `L` | Left Stick X / Y |

### `--input-script <파일>` 형식

무인 실행 중 특정 VBlank에서 버튼을 누르거나 떼려면 `--input-script`에 텍스트 스크립트를 전달한다(가장 최근에 도달한 타임스탬프의 버튼 마스크를 유지).

* 각 줄 형식: `<vblank> <버튼 토큰 ...>` (`#` 이후는 주석, 공백으로 여러 버튼 조합 가능).
* 지원 토큰(대소문자 무관): `START`, `SELECT`, `UP`, `RIGHT`, `DOWN`, `LEFT`, `L`(`LTRIGGER`, `L_TRIGGER`), `R`(`RTRIGGER`, `R_TRIGGER`), `TRIANGLE`, `CIRCLE`, `CROSS`, `SQUARE`, `NONE`(`CLEAR`, `0`, 또는 빈 토큰 목록으로 모든 버튼 해제).

---

## 2. `REPSP_HLE_TRACE` 추적 실행

환경 변수 `REPSP_HLE_TRACE=1`을 설정하면 게스트가 호출하는 모든 HLE 함수의 이름, 인자 4개(`$a0`–`$a3`), 반환값을 순서대로 출력한다. `REPSP_HLE_TRACE_LIMIT`으로 최대 출력 건수를 제한할 수 있다.

```powershell
$env:REPSP_HLE_TRACE = "1"
.\build\win64-release\Release\redmp2.exe roms --variants src\game\dmp2\recomp\variants.json
```

실행 끝에는 항상 다음 요약이 함께 출력된다.

* `HLE Call Count Summary`: 라이브러리·함수별 호출 횟수
* `Guest Threads`: 생성된 게스트 스레드 상태(`module_start`, `user_main` 등)
* `Open Files`: 현재 열려 있는 VFS 파일 목록
* `User Partition Blocks`: 사용자 메모리 파티션 블록 현황

## 3. 게스트 스택 균형·호스트 스택 진단 (`REDMP2_TRACE_GUEST`)

CMake 구성 시 `-DREDMP2_TRACE_GUEST=ON`을 켜면(`OFF`가 기본값이며 비활성 시 오버헤드 0) `src/recomp_abi/trace.c`의 게스트 진단 훅이 활성화됩니다.

* **스택 균형 훅 (`psp_trace_sp*`)**: `PSP_SP_CHECK`(`jr $ra`), `PSP_SP_LEAVE`(블록 경계 넘김), `PSP_SP_CALL`(`jal`/`jalr` 호출 전후)에서 불균형이 생기면 지점·델타별로 최초 1회 표준 오류에 출력하고 횟수를 집계합니다.
* **호스트 스택 최고 사용량**: 게스트 스레드별 호스트 스택 사용량 최고치와 가장 깊었던 4단계 호출 사슬을 기록해 정지 보고서 끝의 `=== Guest SP Balance & Host Stack Summary ===`에 출력합니다.
* **정적 점검 도구 (`tools/lift_audit.py`)**: 생성 코드(`--generated`)와 재배치 이미지(`allegrexrecomp relocate ... --image`)를 대조해 본문 끝 명령과 C 코드 끝 모양의 일치 여부를 전수 점검합니다.
  ```powershell
  python tools\lift_audit.py --generated src\game\dmp2\recomp\generated --image <reloc.bin> --base 0x08804000
  ```

## 4. 오디오·SAS·ATRAC 진단

* **`REPSP_AUDIO_DUMP=<path.wav>`**: 최종 믹서 출력 WAV(`<path.wav>`)와 함께, 실제 출력이 발생한 `sceAudio` 채널별 믹서 입력 WAV(`<stem>.ch<N>.wav`) 및 `__sceSasCore` 믹서 출력 WAV(`<stem>.sas.wav`)를 함께 기록한다. 기록한 WAV에서 양쪽에 소리가 있는 10–50 ms 무음 구간(끊김)을 계측하려면 `python tools/audio_gaps.py <path.wav> [--threshold 32.0]`를 사용한다.
* **`REPSP_SCHED_STATS=1`**: 스케줄러 스레드 기상 지연(원인·우선순위 구간별 평균/최대/히스토그램)을 계측해 정지 보고서 `Scheduler:` 줄에 출력한다.
* **`REPSP_SAS_TRACE=1`**: `Init`, `SetVoice`, `KeyOn`, `KeyOff`, `SetPitch`, 보이스 종료 사유, `GetEndFlag` 변화를 `[SAS_TRACE]`로 출력한다.
* **`REPSP_ATRAC_TRACE=1`**: `Open`, `SetLoopNum`, `ResetPlayPosition`, 디코딩 호출별 상태, `Release`를 `[ATRAC_TRACE]`로 출력한다.

---

# Running and HLE Tracing Guide

This guide describes how to run `redmp2` to inspect kernel/UMD HLE boot behavior and trace NID calls.

## 1. Default Boot Execution

```powershell
.\build\win64-release\Release\redmp2.exe roms --variants src\game\dmp2\recomp\variants.json --headless --max-vblanks 600
```

Pass `--max-vblanks <N>` to stop deterministically after `N` VBlanks, `--renderer <sw|gl>` to select the render backend (`gl` default in windowed mode, `sw` default with `--headless`), `--scale <1..8>` to set window scale and GL internal FBO resolution (`480*scale x 272*scale`, default `2`), `--audio-latency-ms <N>` to set host audio buffer target latency (default `20`), `--input-script <path>` to inject timed controller state, `--save-dir <dir>` to override the host `ms0:/PSP/SAVEDATA` directory, `--dialog-answer <yes|no>` for undrawn yes/no message dialogs (default `yes`), and `--crash-dir <dir>` / `--no-input-record` / `--debug-crash <kind>` for crash capture and input recording (see [crash-reporting.md](crash-reporting.md)).

## 2. Tracing with `REPSP_HLE_TRACE`

Set `REPSP_HLE_TRACE=1` (and optionally `REPSP_HLE_TRACE_LIMIT=<N>`) to log each HLE call with its first four arguments and return value, followed by the kernel call-count, thread, open-file, and user-partition summaries.

## 3. Guest SP Balance and Host Stack Diagnostics (`REDMP2_TRACE_GUEST`)

Configure CMake with `-DREDMP2_TRACE_GUEST=ON` (default `OFF`, zero runtime cost when disabled) to enable the diagnostic hooks in `src/recomp_abi/trace.c` (`psp_trace_sp*` and host stack high-water measurement). Run `python tools/lift_audit.py --generated src/game/dmp2/recomp/generated --image <reloc.bin> --base 0x08804000` for static lift auditing.

## 4. Audio, SAS, and ATRAC Diagnostics

Use `REPSP_AUDIO_DUMP=<path.wav>` (with `tools/audio_gaps.py`), `REPSP_SCHED_STATS=1`, `REPSP_SAS_TRACE=1`, and `REPSP_ATRAC_TRACE=1`.
