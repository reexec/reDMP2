# 작업 지시: repsp v0.0.30 고정과 첫 런타임 부팅 측정 (issue #3)

설계: [20261121-i003](../design/20261121-i003-repsp-exec-load.md). 브랜치: `i003-repsp-exec-load`. 커밋 제목 끝에 `(#3)`.

1. `third_party/repsp`를 v0.0.30 머지 커밋(리드가 알려 준 커밋)으로 옮긴다(`git -C third_party/repsp checkout <commit>` 후 상위 저장소에서 `git add third_party/repsp`). repsp 안의 파일은 고치지 않는다.
2. 생성 코드 없이 Win64 Debug/Release 빌드와 CTest(설계 §5의 1). 생성 코드 디렉터리는 잠시 다른 곳으로 옮기거나 `-DREDMP2_GENERATED_DIR`로 빈 디렉터리를 가리켜 확인한다.
3. 생성 코드 포함 Release 빌드와 CTest, 헤드리스 실행(설계 §5의 2·3). 실행에는 반드시 `--save-dir`와 `--crash-dir`를 저장소 밖 임시 디렉터리로 준다. 정지 보고 전문은 커밋하지 않는다.
4. 문서: 설계 §4. 한국어 먼저, 영어 번역이 바로 아래. 확인됨/추정/미확정 구분.
5. 작업 로그 `docs/work-logs/20261121-i003-repsp-exec-load.md`: 명령, 수치, 정지 보고 요약, 다음 HLE 후보(정적 대조 결과와 첫 정지의 관계).
6. `git status`로 게임 데이터·생성 코드가 없는지 확인하고 커밋.

금지: push, PR, 태그, issue 생성. repsp·psprecomp 안의 수정. HLE 구현.

---

# Work order: pin repsp v0.0.30 and measure the first runtime boot (issue #3)

Design: [20261121-i003](../design/20261121-i003-repsp-exec-load.md). Branch: `i003-repsp-exec-load`. Commit titles end with `(#3)`.

(1) Move `third_party/repsp` to the v0.0.30 merge commit the lead gives (`git -C third_party/repsp checkout <commit>`, then `git add third_party/repsp` in the superproject); no edits inside repsp. (2) Win64 Debug/Release without generated code, with CTest (design §5 item 1); move the generated directory aside temporarily or point `-DREDMP2_GENERATED_DIR` at an empty directory. (3) Release with generated code, CTest and the headless run (design §5 items 2 and 3); always pass `--save-dir` and `--crash-dir` pointing at a temporary directory outside the repository; the full stop report is not committed. (4) Documents: design §4, Korean first with the English right below, confirmed/inferred/unresolved marked. (5) Work log `docs/work-logs/20261121-i003-repsp-exec-load.md`: commands, numbers, the stop report summary, the next HLE candidates (how the static coverage relates to the first stop). (6) Check `git status` for game data and generated code, then commit.

Not allowed: push, pull request, tag, issue; edits inside repsp or psprecomp; HLE implementation.
