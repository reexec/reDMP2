# 작업 로그: 곡 선택 화면 곡 목록 글자 깨짐 (issue #15)

* 이슈: [#15](https://github.com/reexec/reDMP2/issues/15) (연계: [reexec/repsp#75](https://github.com/reexec/repsp/issues/75))
* 브랜치: `i015-song-list-text`

## 1. 현상과 원인 (확인됨)

* 곡 선택 화면 오른쪽 곡 목록 글자가 알아볼 수는 있지만 픽셀 단위로 깨졌다. headless 소프트웨어 렌더링과 창 모드 GL 모두 같았다.
* repsp #75의 GE 추적(`REPSP_GE_TRACE`)으로 곡 목록이 DXT5 텍스처(256×256)로 그려짐을 확인했다. 추적이 내보낸 디코드 이미지가 화면과 똑같이 깨져 있었고, 원시 바이트를 올바른 순서로 디코드하면 깨끗했다. 게스트 데이터나 lift 결함이 아니었다.
* 원인: repsp `texture_decode.cpp`의 DXT5 48비트 알파 인덱스 조합에서 하위 32비트와 상위 16비트가 뒤바뀜. repsp v0.0.35에서 수정했다.

## 2. 확인

* 리뷰어 headless 재현: 곡 선택 화면의 곡 이름(6개)이 선명하다. 덤프는 커밋하지 않았다.
* 사용자 창 모드 확인: 정상.
* 이 저장소의 코드 변경은 없다(repsp 고정점 이동만).

## 3. 관찰

* `--input-script`의 입력은 vblank 번호로 들어가지만 게임이 입력을 읽는 시점은 프레임 속도에 따라 달라진다. 같은 스크립트로도 실행마다 다른 화면에서 멈출 수 있다(한 번은 이름 입력 화면, 다시 돌리니 곡 선택). 회귀 확인 자동화 전에 다룰 문제다.

---

# Work log: garbled song list text on the song select screen (issue #15)

* Issue: [#15](https://github.com/reexec/reDMP2/issues/15) (paired: [reexec/repsp#75](https://github.com/reexec/repsp/issues/75))
* Branch: `i015-song-list-text`

## 1. Symptom and cause (confirmed)

* The song list text on the right of the song select screen was recognizable but garbled at the pixel level, identically in headless software rendering and windowed GL.
* repsp #75's GE trace (`REPSP_GE_TRACE`) showed the list is drawn from a DXT5 texture (256×256).
  * The decoded image the trace exported was garbled exactly like the screen.
  * Decoding the raw bytes in the right order gave clean text.
  * So it was neither the guest data nor a lift defect.
* Cause: repsp `texture_decode.cpp` swapped the low 32 bits and high 16 bits when assembling DXT5's 48-bit alpha indices. Fixed in repsp v0.0.35.

## 2. Checks

* Reviewer headless reproduction: the six song names on the song select screen are clean. Dumps are not committed.
* User check in windowed mode: fine.
* No code changes in this repository; only the repsp pin moves.

## 3. Observation

* `--input-script` inputs are keyed to vblank numbers, but when the game reads input depends on frame speed. The same script can end on different screens from run to run (once the name entry screen, then song select on a rerun). This should be handled before automating regression checks.
