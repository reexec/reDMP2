# 작업 로그: 아날로그 레인 자동 플레이 (issue #19)

* 이슈: [#19](https://github.com/reexec/reDMP2/issues/19) (연관 repsp 이슈: [reexec/repsp#77](https://github.com/reexec/repsp/issues/77))
* 설계: [20261122-i019-analog-autoplay.md](../design/20261122-i019-analog-autoplay.md)
* 분석: [autoplay.md](../analysis/autoplay.md) (§6)
* 브랜치: `i019-analog-autoplay` (개발·검증은 `third_party/repsp`를 `i77-sample-filter` 작업 트리로 둔 채 수행했으며, 서브모듈 포인터는 커밋하지 않음)


## 리뷰어 측정: 느린 프레임에서의 한계

* 리뷰어가 같은 4B 곡을 headless로 두 번 더 돌렸다. 프레임 간격이 평균 약 65 ms로 느렸고, 결과는 `BREAK 5`와 `BREAK 21`이었다(둘 다 STAGE CLEARED).
* GL 창 모드(평균 21.8 ms)에서는 콤보가 끊기지 않고 166까지 MAX 100%로 이어졌다.
* 원인: 단타는 누름 → 뗌에 호출 두 번이 필요하다. 프레임이 65 ms면 130 ms가 되어, 같은 레인의 촘촘한 노트 간격보다 길어진다. 버튼 레인 로직은 v0.0.9와 같다(이번 변경과 무관).
* 한계: 프레임이 크게 떨어지는 환경에서는 자동 플레이도 노트를 놓칠 수 있다. 보통 창 모드(60 fps)에서는 문제없다.

---

## 1. 단위별 변경 요약

### 1단계: 아날로그 입력·판정 분석 (`534bcbf`)
* `docs/analysis/autoplay.md` (§6 추가):
  * `0x088320A4(Lx, Ly)`의 우세 축 판정(`|Lx-128| > |Ly-128|`), 임계값(`>= 193`, `< 64`), 중립 범위(`64..192`)와 `0x08976851`(활동 카운터)·`0x08976850`(30프레임 감쇠 타이머) 갱신 규칙(방향 전환 → 카운터 `+1`·타이머 `30`, 같은 방향 → 타이머 `-1`, 중립 → 타이머 `-2`)을 생성 코드에서 확인했다.
  * `0x088585E8` / `0x08858624` / `0x08858CFC`가 아날로그 방향을 구분하지 않고 카운터 `!= 0`만 판정함을 확인했다.
  * 5B `sO mUCH iN LUV` NM(vblank 4200 라이브 RAM 덤프)에서 아날로그 레인(`lane 5`, `ch=9`)의 플레이 노트 3개가 모두 `type == 1`, `duration >= 7` 롱노트임을 실측했다.

### 2단계: 샘플 필터 기반 아날로그 레인 자동 플레이 구현 (`ee5e087`)
* `src/app/autoplay.h`, `src/app/autoplay.cpp`:
  * `FilterButtons`를 `FilterSample(repsp::hle::ctrl::SceCtrlData& sample, const repsp::runtime::GuestMemory& mem)`로 교체하고, 기존 곡별 "아날로그 미지원" 로그(`CheckAnalogLaneNotesOnce`)를 제거했다.
  * 설계 §3·§3.1 결정에 따라 아날로그 레인(`gear+5756 - 1`) 커서와 유지 상태를 추가했다:
    * 플레이 장면에서 자동 플레이가 켜져 있으면 사람 스틱 입력을 중립 `(128, 128)`으로 지우고(일시정지 시에는 변경 없이 통과 및 유지 해제), 헤더(`type == 4`)와 이미 판정된 노트(`+6 != 0`)는 건너뛴다.
    * 노트 프레임(`note_time <= now + delta/2`)에 오른쪽 `(255, 128)`을 넣는다.
    * 롱노트(`duration >= 7`)는 끝 시각(`note_time + duration`)까지 호출마다 왼쪽 `(0, 128)`과 오른쪽 `(255, 128)`을 교대로 넣어 `0x088320A4`의 30프레임 타이머를 계속 갱신한다.
    * 단타는 누른 다음 호출에 중립 `(128, 128)`으로 되돌리며, 노트 전에도 중립을 유지한다.
* `src/app/main.cpp`:
  * `repsp::hle::ctrl::SetButtonFilter` 대신 `repsp::hle::ctrl::SetSampleFilter`를 연결하고, 종료·오류 경로에서 `SetSampleFilter(nullptr)`로 해제한다.
* `src/app/autoplay_probe.cpp`:
  * 기존 항목 1~6을 `FilterSample` 호출로 갱신하고, 항목 7(아날로그 레인: 비플레이 장면 스틱 무변경, 노트 전 중립 유지·사람 스틱 제거, 이미 판정된 아날로그 노트 건너뛰기, 단타 누름·중립 복귀, 롱노트 좌/우 교대 유지·끝 시각 중립 복귀)을 추가했다.

---

## 2. 빌드 및 CTest 결과 (**확인됨**)

모든 구성은 `third_party/repsp` 브랜치 `i77-sample-filter`(`ab93ae8`)를 작업 트리에 둔 상태에서 수행했다.

* **Win64 Release (생성 코드 포함 + FFmpeg 플러그인, `build\win64-release-ffmpeg`)**:
  * 프로젝트 소스 컴파일러 경고 **0건**.
  * CTest **34/34 통과** (`redmp2_autoplay_probe` 항목 1~7 및 `repsp` `ctrl_probe` 항목 9 포함).
* **Win64 Debug (생성 코드 미포함, `REDMP2_GENERATED_DIR=<empty>`)**:
  * 프로젝트 소스 컴파일러 경고 **0건**.
  * 앱/프로브 CTest **10/10 통과**.
* **Win64 Release (생성 코드 미포함, `REDMP2_GENERATED_DIR=<empty>`)**:
  * 프로젝트 소스 컴파일러 경고 **0건**.
  * 앱/프로브 CTest **10/10 통과**.
* **Linux x64**: CI에서 검증.

---

## 3. 실기 검증 결과 (`--headless --autoplay`, `build\win64-release-ffmpeg`) (**확인됨**)

모든 실행에서 `--save-dir`, `--crash-dir`, `REPSP_FRAME_DUMP`는 저장소 밖 임시 디렉터리를 사용했으며 덤프는 커밋하지 않았다. 헤드리스 소프트웨어 렌더링 프레임 간격이 호스트 CPU 부하에 영향을 받으므로, 각 모드는 단독 실행으로 측정했다.

| 모드 | 검증 곡 | `--max-vblanks` | 결과 화면 (덤프 확인) | 비고 | 디스패치 미스 / 종료 코드 |
|---|---|---:|---|---|---|
| **5B** | `sO mUCH iN LUV` NM | `12500` | `STAGE CLEARED!!` / `COMBO 758` / `BREAK 0` / `JUDGMENT 99.7%` / `SCORE 57624` / `GOLD 1748` / `A+` (`frame_4730.bmp`) | 기존 #17(`COMBO 449` / `BREAK 3`)에서 아날로그 롱노트 3개 모두 성공 | `0` / `0` |
| **4B** | `Ladymade Star` NM | `11800` | `STAGE CLEARED!!` / `COMBO 302` / `BREAK 0` / `JUDGMENT 100.0%` / `SCORE 19400` / `GOLD 1750` / `S` (`frame_4550.bmp`) | 회귀 확인 (`interval mean=40.50 ms`, GE `>=50 ms` 0건) | `0` / `0` |
| **6B** | `Stay with me` NM | `12500` | `STAGE CLEARED!!` / `COMBO 345` / `BREAK 0` / `JUDGMENT 99.3%` / `SCORE 22499` / `GOLD 1467` / `A+` (`MAX COMBO`, `frame_5350.bmp`) | 회귀 확인 (`interval mean=35.58 ms`, GE `>=50 ms` 0건) | `0` / `0` |

---

# Work Log: Autoplay for Analog-Lane Notes (issue #19)

* Issue: [#19](https://github.com/reexec/reDMP2/issues/19) (paired repsp issue: [reexec/repsp#77](https://github.com/reexec/repsp/issues/77))
* Design: [20261122-i019-analog-autoplay.md](../design/20261122-i019-analog-autoplay.md)
* Analysis: [autoplay.md](../analysis/autoplay.md) (§6)
* Branch: `i019-analog-autoplay` (developed and verified with `third_party/repsp` checked out on `i77-sample-filter`; the submodule pointer is intentionally not committed)

---

## 1. Summary of Changes by Unit

### Phase 1: Analog Input and Judgement Analysis (`534bcbf`)
* `docs/analysis/autoplay.md` (added §6):
  * Confirmed `0x088320A4(Lx, Ly)` dominant-axis test (`|Lx-128| > |Ly-128|`), thresholds (`>= 193`, `< 64`), neutral range (`64..192`), and counter/timer updates at `0x08976851` / `0x08976850` (direction change → counter `+1`, timer `30`; same direction → timer `-1`; neutral → timer `-2`) in generated code.
  * Confirmed that `0x088585E8` / `0x08858624` / `0x08858CFC` check only `counter != 0` rather than a specific stick direction.
  * Measured the 3 analog-lane (`lane 5`, `ch=9`) playable notes in 5B `sO mUCH iN LUV` NM from a live vblank 4200 RAM dump and confirmed all 3 are `type == 1`, `duration >= 7` long notes.

### Phase 2: Analog-Lane Autoplay via `SetSampleFilter` (`ee5e087`)
* `src/app/autoplay.h`, `src/app/autoplay.cpp`:
  * Replaced `FilterButtons` with `FilterSample(repsp::hle::ctrl::SceCtrlData& sample, const repsp::runtime::GuestMemory& mem)` and removed the per-song "analog input is not handled yet" log (`CheckAnalogLaneNotesOnce`).
  * Added the analog-lane (`gear+5756 - 1`) cursor and hold state per design §3 and §3.1:
    * Clears human stick input to neutral `(128, 128)` during gameplay when autoplay is enabled (passing through unchanged and dropping holds on pause), and skips headers (`type == 4`) and already-judged notes (`+6 != 0`).
    * Sets right `(255, 128)` on the note frame (`note_time <= now + delta/2`).
    * For long notes (`duration >= 7`), alternates left `(0, 128)` and right `(255, 128)` on each call until `note_time + duration` so `0x088320A4`'s 30-frame timer keeps refreshing.
    * For taps, returns to neutral `(128, 128)` on the next call, and holds neutral before the next note.
* `src/app/main.cpp`:
  * Switched `repsp::hle::ctrl::SetButtonFilter` to `repsp::hle::ctrl::SetSampleFilter` and cleared with `SetSampleFilter(nullptr)` on exit and error paths.
* `src/app/autoplay_probe.cpp`:
  * Updated Checks 1–6 to call `FilterSample`, and added Check 7 covering non-gameplay stick passthrough, pre-note neutral clearing, already-judged analog note skipping, tap press and neutral return, and long-note alternating hold and release.

---

## 2. Build and CTest Results (**confirmed**)

All builds were run with `third_party/repsp` checked out on branch `i77-sample-filter` (`ab93ae8`).

* **Win64 Release (with generated code + FFmpeg plugin, `build\win64-release-ffmpeg`)**:
  * Project source compiler warnings: **0**.
  * CTest: **34/34 passed** (including `redmp2_autoplay_probe` Checks 1–7 and `repsp` `ctrl_probe` Check 9).
* **Win64 Debug (without generated code, `REDMP2_GENERATED_DIR=<empty>`)**:
  * Project source compiler warnings: **0**.
  * App/probe CTest: **10/10 passed**.
* **Win64 Release (without generated code, `REDMP2_GENERATED_DIR=<empty>`)**:
  * Project source compiler warnings: **0**.
  * App/probe CTest: **10/10 passed**.
* **Linux x64**: Verified in CI.

---

## 3. Game Verification (`--headless --autoplay`, `build\win64-release-ffmpeg`) (**confirmed**)

All runs placed `--save-dir`, `--crash-dir`, and `REPSP_FRAME_DUMP` outside the repository; no dumps were committed. Because headless software rendering frame intervals are sensitive to host CPU load, each mode was verified in a solo run.

| Mode | Song | `--max-vblanks` | Result screen (from dump) | Notes | Dispatch misses / Exit |
|---|---|---:|---|---|---|
| **5B** | `sO mUCH iN LUV` NM | `12500` | `STAGE CLEARED!!` / `COMBO 758` / `BREAK 0` / `JUDGMENT 99.7%` / `SCORE 57624` / `GOLD 1748` / `A+` (`frame_4730.bmp`) | All 3 analog long notes succeeded (was `COMBO 449` / `BREAK 3` in #17) | `0` / `0` |
| **4B** | `Ladymade Star` NM | `11800` | `STAGE CLEARED!!` / `COMBO 302` / `BREAK 0` / `JUDGMENT 100.0%` / `SCORE 19400` / `GOLD 1750` / `S` (`frame_4550.bmp`) | Regression pass (`interval mean=40.50 ms`, 0 GE runs `>=50 ms`) | `0` / `0` |
| **6B** | `Stay with me` NM | `12500` | `STAGE CLEARED!!` / `COMBO 345` / `BREAK 0` / `JUDGMENT 99.3%` / `SCORE 22499` / `GOLD 1467` / `A+` (`MAX COMBO`, `frame_5350.bmp`) | Regression pass (`interval mean=35.58 ms`, 0 GE runs `>=50 ms`) | `0` / `0` |


## Reviewer measurement: limits with slow frames

* The reviewer ran the same 4B song headless twice more. Frames averaged about 65 ms, and the results were `BREAK 5` and `BREAK 21` (both STAGE CLEARED).
* In GL windowed mode (mean 21.8 ms) the combo ran unbroken to 166 at MAX 100%.
* Cause: a tap needs two calls, press then release. At 65 ms per frame that is 130 ms, longer than the gap between dense notes on one lane. The button-lane logic is the same as v0.0.9 (unrelated to this change).
* Limit: where the frame rate drops sharply, autoplay can miss notes too. Normal windowed play (60 fps) is fine.
