# 분석 문서 색인

원본 디스크와 실행 파일에서 확인한 사실(**확인됨**), 가설(**추정**), 미해결 질문(**미확정**)을 주제별로 둔다.

| 문서 | 내용 |
|---|---|
| [disc-image.md](disc-image.md) | ULKS-46116 원본 UMD의 신원(redump #33115)과 파일 구성 |
| [executable.md](executable.md) | 원본 실행 파일(`BOOT.BIN`, `ET_EXEC`) 구조, 세그먼트, 리프트 커버리지(97.5%) 및 `lift_audit.py` 결과 |
| [boot.md](boot.md) | 첫 부팅 측정(`LoadPrx` `ET_EXEC` 거부) 및 `tools/import_coverage.py` 정적 임포트 커버리지(141/229) |
| [autoplay.md](autoplay.md) | 자동 플레이 구현을 위한 DMP2 패드·키 배치·판정·장면·채보·노트 메모리 구조 분석 |
| [link-disc.md](link-disc.md) | 링크 디스크(UMD 교체) 메뉴, UMD·HLE 호출 순서, 교체 디스크 파일 접근 및 `ms0:`/`sceUtility` 경로 분석 |

---

# Analysis index

Findings from the original disc and executable, by topic, marked **confirmed**, **inferred** or **unresolved**.

| Document | Contents |
|---|---|
| [disc-image.md](disc-image.md) | Identity (redump #33115) and file layout of the original ULKS-46116 UMD |
| [executable.md](executable.md) | Structure of `BOOT.BIN` (`ET_EXEC`), segments, lift coverage (97.5%), and `lift_audit.py` summary |
| [boot.md](boot.md) | First boot measurement (`LoadPrx` `ET_EXEC` rejection) and `tools/import_coverage.py` static import coverage (141/229) |
| [autoplay.md](autoplay.md) | DMP2 pad, key-config, judgement, scene, chart, and note memory layout analysis for autoplay |
| [link-disc.md](link-disc.md) | Link Disc (UMD swap) menu, UMD/HLE call sequence, swapped-disc file access, and `ms0:`/`sceUtility` path analysis |
