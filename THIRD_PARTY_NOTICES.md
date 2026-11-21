# 서드파티 고지 (Third-Party Notices)

reDMP2는 다음 서드파티 오픈소스 프로젝트를 포함하거나 링크합니다. 저장소 골격 및 `src/recomp_abi/` 어댑터는 [reexec/reDMP1](https://github.com/reexec/reDMP1)(MIT License)에서 이식했습니다.

## 1. psprecomp

* 위치: `third_party/psprecomp` (submodule) 및 `src/recomp_abi/` (일부 코드 이식)
* 업스트림: https://github.com/sp00nznet/psprecomp
* 고정점: `5ca18fa`
* 라이선스: MIT License (Copyright (c) 2024 sp00nznet)
* 용도: 정적 재컴파일 툴킷(`allegrexrecomp` 호스트 CLI 도구 및 ABI 헤더), `src/vfpu.c` 및 `src/dispatch.c` 일부를 `src/recomp_abi/`에 포함.

## 2. repsp

* 위치: `third_party/repsp` (submodule)
* 업스트림: https://github.com/reexec/repsp
* 고정점: `a243bed` (v0.0.33)
* 라이선스: MIT License (Copyright (c) 2026 reexec)
* 용도: 공유 PSP 런타임, 게스트 메모리 변환, NID 레지스트리, HLE 모듈, 플랫폼 백엔드.

### 2.1. Simple DirectMedia Layer (SDL3) (repsp 하위 의존성)

* 위치: `third_party/repsp/third_party/SDL`
* 업스트림: https://github.com/libsdl-org/SDL
* 고정점: `ee239da42bd043039fddbad0622645f54bf5a640` (`release-3.4.x`)
* 라이선스: zlib License (Copyright (C) 1997-2026 Sam Lantinga)
* 용도: 플랫폼 창, 입력 이벤트, 오디오 출력.

### 2.2. Dear ImGui (repsp 하위 의존성)

* 위치: `third_party/repsp/third_party/imgui`
* 업스트림: https://github.com/ocornut/imgui
* 고정점: `c1aaac65e896a45f81f47a4d68f9959e0f86c859`
* 라이선스: MIT License (Copyright (c) 2014-2026 Omar Cornut)
* 용도: OpenGL 렌더러 내부 설정 메뉴 및 통계 오버레이.

## 3. FFmpeg (선택적 동적 코덱 플러그인 `repsp_codec_ffmpeg`)

* 업스트림: https://ffmpeg.org/
* 라이선스: GNU Lesser General Public License (LGPL) version 2.1 이상 (GPL 빌드 금지)
* 용도: ATRAC3/ATRAC3plus 오디오 및 H.264 동영상 디코딩을 위한 선택적 런타임 로드 플러그인(`REDMP2_CODEC_FFMPEG=ON`, 기본값 `OFF`). FFmpeg 소스와 바이너리는 이 저장소에 포함하지 않습니다.
* 배포 요구사항(릴리즈에 FFmpeg LGPL 공유 라이브러리를 함께 포함할 경우):
  1. 본 고지와 해당 LGPL 라이선스 전문(`LICENSE.txt` / `COPYING.LGPL*`)을 동봉합니다.
  2. 사용한 BtbN `lgpl-shared` 릴리즈 태그에 대응하는 FFmpeg 소스 코드 또는 소스 제공 안내를 포함합니다.
  3. 사용자가 `redmp2`를 다시 빌드하지 않고도 `avcodec-*.dll` / `avutil-*.dll` / `swresample-*.dll`(또는 `.so`)을 교체할 수 있도록 동적 링크를 유지합니다.

---

# Third-Party Notices (English)

reDMP2 incorporates or links the following third-party open-source projects. The repository skeleton and `src/recomp_abi/` adapter were ported from [reexec/reDMP1](https://github.com/reexec/reDMP1) (MIT License).

## 1. psprecomp

* Location: `third_party/psprecomp` (submodule) and `src/recomp_abi/` (carried code)
* Upstream: https://github.com/sp00nznet/psprecomp
* Pin: `5ca18fa`
* License: MIT License (Copyright (c) 2024 sp00nznet)
* Usage: Static recompilation toolkit (`allegrexrecomp` host CLI tool and ABI headers); portions of `src/vfpu.c` and `src/dispatch.c` carried into `src/recomp_abi/`.

## 2. repsp

* Location: `third_party/repsp` (submodule)
* Upstream: https://github.com/reexec/repsp
* Pin: `a243bed` (v0.0.33)
* License: MIT License (Copyright (c) 2026 reexec)
* Usage: Shared portable PSP runtime, guest memory translation, NID registry, HLE modules, platform backends.

### 2.1. Simple DirectMedia Layer (SDL3) (repsp sub-dependency)

* Location: `third_party/repsp/third_party/SDL`
* Upstream: https://github.com/libsdl-org/SDL
* Pin: `ee239da42bd043039fddbad0622645f54bf5a640` (`release-3.4.x`)
* License: zlib License (Copyright (C) 1997-2026 Sam Lantinga)
* Usage: Platform windowing, input events, and audio output.

### 2.2. Dear ImGui (repsp sub-dependency)

* Location: `third_party/repsp/third_party/imgui`
* Upstream: https://github.com/ocornut/imgui
* Pin: `c1aaac65e896a45f81f47a4d68f9959e0f86c859`
* License: MIT License (Copyright (c) 2014-2026 Omar Cornut)
* Usage: In-game settings menu and stats overlay under the OpenGL renderer.

## 3. FFmpeg (Optional Dynamic Codec Plugin `repsp_codec_ffmpeg`)

* Upstream: https://ffmpeg.org/
* License: GNU Lesser General Public License (LGPL) version 2.1 or later (GPL builds are prohibited)
* Usage: Optional runtime-loaded plugin (`REDMP2_CODEC_FFMPEG=ON`, `OFF` by default) for ATRAC3/ATRAC3plus audio and H.264 video decoding. Neither FFmpeg source nor binaries are stored in this repository.
* Distribution requirements (when bundling FFmpeg LGPL shared libraries in a release):
  1. Include this notice and the corresponding LGPL license text (`LICENSE.txt` / `COPYING.LGPL*`).
  2. Provide corresponding FFmpeg source code or build-pin / source offer instructions (matching the exact BtbN `lgpl-shared` release tag).
  3. Keep dynamic linking replaceable so users can swap `avcodec-*.dll` / `avutil-*.dll` / `swresample-*.dll` (or `.so` equivalents) without rebuilding `redmp2`.
