# 설계: 아날로그 레인 자동 플레이 (issue #19)

## 1. 배경

* v0.0.9의 자동 플레이(#17)는 버튼 레인만 친다.
* 5B `sO mUCH iN LUV` NM에서 아날로그 레인 노트 3개가 BREAK로 남았다. 로그와 BREAK 수가 같았다.
* 아날로그 레인은 `gear+5756 - 1`번 레인이다. 판정 함수 `0x0885829C`가 이 레인을 `analog_bits`와 `G+125`(이전 값)로 판정한다([분석](../analysis/autoplay.md) §3).
* `analog_bits`는 `0x08832210()`이 돌려주는 `0x08976851`이다. 이 값은 `0x088320A4(Lx, Ly)`가 갱신한다(분석 §1). 리뷰어가 생성 코드에서 확인한 것: `0x088320A4`는 `Lx-128`, `Ly-128`의 크기를 비교하고 193/64 같은 임계값과 비교한다.

## 2. 1단계: 분석 (먼저, 리뷰 후 2단계)

`docs/analysis/autoplay.md`에 절을 더한다. 항목마다 확인됨/추정과 근거를 단다.

1. `0x088320A4`가 `Lx`/`Ly`에서 `0x08976851`을 만드는 규칙: 방향별 비트, 임계값, 우세 축 판정, 중립 범위.
2. 판정 함수의 아날로그 레인 분기: 어떤 값 변화가 "눌림"인지, 유지와 뗌은 어떻게 판정하는지.
3. 노트가 방향을 요구하는지: 노트의 어떤 필드가 방향을 정하는지, 또는 어느 방향이든 되는지.
4. 아날로그 롱노트가 있는지, 있다면 유지 조건.
5. 실측: 아날로그 노트가 있는 곡(5B `sO mUCH iN LUV` NM)에서 그 노트들의 필드 값.

**1단계가 끝나면 멈추고 보고한다.**

## 3. 2단계: 구현

* repsp [#77](https://github.com/reexec/repsp/issues/77)의 `SetSampleFilter`를 쓴다. 소비자는 버튼 필터 대신 샘플 필터 하나로 버튼과 스틱을 함께 정한다.
* `Autoplay`에 다음을 더한다:
  * 아날로그 레인의 커서를 둔다. 노트 시각 규칙은 버튼 레인과 같다(`now + d/2`, 이미 판정된 노트는 건너뜀).
  * 누를 프레임에는 그 노트가 요구하는 방향의 `Lx`/`Ly` 값을 넣는다. 1단계에서 찾은 임계값을 충분히 넘는 값을 쓴다.
  * 단타는 다음 호출에 중립(128)으로 되돌린다. 롱노트는 끝까지 유지한다.
* 자동 플레이가 켜져 있고 플레이 장면이면, 사람의 스틱 입력은 중립으로 지운다. 버튼 레인과 같은 규칙이다.
* 곡마다 남기던 "아날로그 노트 미지원" 로그는 지운다.
* `redmp2_autoplay_probe`에 아날로그 케이스를 더한다:
  * 방향별 값
  * 단타의 누름과 되돌림
  * 롱노트 유지
  * 비플레이 장면에서는 손대지 않음

### 3.1 1단계 리뷰 뒤 결정 (2026-11-23)

분석은 [`docs/analysis/autoplay.md`](../analysis/autoplay.md) §6에 있다. 리뷰어는 `0x088320A4`의 방향 판정과 카운터·타이머 갱신을 생성 코드에서 확인했다.
* 방향 전환이면 카운터 `+1`, 타이머 30
* 같은 방향이면 타이머 `-1`, 0이면 카운터 0
* 중립이면 타이머 `-2`

판정은 카운터 `!= 0`만 본다. 아날로그 레인은 방향 입력이 아니라 "스틱을 흔드는" 입력이다. 그래서 §3의 "노트가 요구하는 방향"을 다음으로 바꾼다.

* **누름**: 노트 프레임에 중립이 아닌 방향을 넣는다. 예: 오른쪽 `(255, 128)`. 직전 방향이 중립이면 전환이 되어 카운터가 0이 아니게 된다.
* **유지(롱노트)**: 끝 시각까지 호출마다 방향을 오른쪽과 왼쪽 `(0, 128)`으로 번갈아 넣는다. 전환마다 타이머가 30으로 돌아가 카운터가 0이 되지 않는다.
* **단타**: 누른 다음 호출에 중립 `(128, 128)`으로 돌린다.
* **노트 전**: 중립을 유지한다. 앞 노트의 카운터가 다음 노트 전에 0으로 떨어져야 다음 노트가 일찍 판정되지 않는다. 중립에서는 최대 15프레임 안에 0이 된다.
* 스틱 값은 샘플 필터(repsp #77)로 넣는다. 버튼도 같은 필터에서 정한다.

## 4. 검증

1. 5B `sO mUCH iN LUV` NM: BREAK 0, 콤보가 노트 수와 같다.
2. 4B·6B 회귀: BREAK 0.
3. CTest: 생성 코드 없이 Win64 Debug/Release, 생성 코드 포함 Release. Linux는 CI.
4. 사용자가 창 모드에서 확인한다.

---

# Design: autoplay for analog-lane notes (issue #19)

## 1. Background

* v0.0.9's autoplay (#17) hits button lanes only.
* On 5B `sO mUCH iN LUV` NM, three analog-lane notes stayed as BREAK, matching the log count.
* The analog lane is lane `gear+5756 - 1`. The judgement function `0x0885829C` judges it from `analog_bits` and `G+125` (the previous value) ([analysis](../analysis/autoplay.md) §3).
* `analog_bits` is `0x08976851`, returned by `0x08832210()` and updated by `0x088320A4(Lx, Ly)` (analysis §1). The reviewer confirmed in the generated code that `0x088320A4` compares the magnitudes of `Lx-128` and `Ly-128`, and compares them against thresholds such as 193 and 64.

## 2. Phase 1: analysis (first; phase 2 after review)

Add a section to `docs/analysis/autoplay.md`, marking each item confirmed/inferred with evidence.

1. The rule by which `0x088320A4` builds `0x08976851` from `Lx`/`Ly`: per-direction bits, thresholds, the dominant-axis test, and the dead zone.
2. The judgement function's analog-lane branch: which change in the value counts as a press, and how hold and release are judged.
3. Whether a note requires a direction: which note field sets it, or whether any direction works.
4. Whether analog long notes exist, and if so their hold condition.
5. Measurement: the field values of the analog notes in a song that has them (5B `sO mUCH iN LUV` NM).

**Stop and report when phase 1 is done.**

## 3. Phase 2: implementation

* Use repsp [#77](https://github.com/reexec/repsp/issues/77)'s `SetSampleFilter`. The consumer sets buttons and stick together through one sample filter instead of the button filter.
* `Autoplay` gains the following:
  * A cursor for the analog lane. The note-time rule is the same as for button lanes (`now + d/2`, already-judged notes skipped).
  * On the press frame, it sets the `Lx`/`Ly` values for the direction the note requires, well past the thresholds found in phase 1.
  * A tap returns to neutral (128) on the next call. A long note is held to its end.
* While autoplay is on in the play scene, human stick input is cleared to neutral, the same rule as for button lanes.
* The per-song "analog notes not supported" log is removed.
* `redmp2_autoplay_probe` gains analog cases:
  * per-direction values
  * tap press and return to neutral
  * long-note hold
  * no change outside the play scene

### 3.1 Decisions after the phase 1 review (2026-11-23)

The analysis is in [`docs/analysis/autoplay.md`](../analysis/autoplay.md) §6. The reviewer confirmed `0x088320A4`'s direction test and its counter/timer updates in the generated code:
* a direction change adds 1 to the counter and sets the timer to 30
* the same direction subtracts 1 from the timer, and the counter becomes 0 when the timer reaches 0
* neutral subtracts 2 from the timer

Judgement looks only at counter `!= 0`. The analog lane is not a direction input but a "wiggle the stick" input, so §3's "direction the note requires" becomes the following:

* **Press**: on the note frame, set a non-neutral direction, for example right `(255, 128)`. Coming from neutral, that is a change, so the counter becomes nonzero.
* **Hold (long note)**: until the end time, alternate right and left `(0, 128)` on each call. Every change resets the timer to 30, so the counter never reaches 0.
* **Tap**: return to neutral `(128, 128)` on the next call.
* **Before a note**: stay neutral. The previous note's counter must fall to 0 before the next note, or the next note is judged early. At neutral it reaches 0 within 15 frames.
* Stick values go through the sample filter (repsp #77), which also sets the buttons.

## 4. Verification

1. 5B `sO mUCH iN LUV` NM: BREAK 0, and the combo equals the note count.
2. 4B and 6B regression: BREAK 0.
3. CTest: Win64 Debug/Release without generated code, and Release with it. Linux in CI.
4. The user checks in windowed mode.
