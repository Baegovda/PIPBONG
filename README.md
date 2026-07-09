<div align="center">

<img src="resources/Pipbong.ico" width="120" alt="PIPBONG logo" />

# PIPBONG

**블록으로 조립하는 Windows 화면 자동화 도구**

_이미지를 찾고, 클릭하고, 키를 누르고, 기다린다 — 코드 한 줄 없이._

<br />

[![Release](https://img.shields.io/github/v/release/Baegovda/PIPBONG?style=for-the-badge&label=최신%20버전&color=38bdf8)](https://github.com/Baegovda/PIPBONG/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/Baegovda/PIPBONG/total?style=for-the-badge&label=다운로드&color=22c55e)](https://github.com/Baegovda/PIPBONG/releases)
[![Changelog](https://img.shields.io/badge/업데이트_로그-한글-a855f7?style=for-the-badge)](UpdateLog/update_log.md)
[![Platform](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/Baegovda/PIPBONG/releases/latest)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Qt](https://img.shields.io/badge/Qt-6-41CD52?style=for-the-badge&logo=qt&logoColor=white)](https://www.qt.io/)

</div>

---

## PIPBONG이 뭔가요?

PIPBONG은 **원하는 프로그램 창**을 대상으로, 사람이 하던 반복 작업을 대신 해주는 데스크톱 자동화 유틸리티입니다.
화면 속 이미지를 눈으로 찾듯 **템플릿 매칭**으로 찾아내고, 그 위치를 클릭하거나 키를 눌러줍니다.

복잡한 스크립트 언어를 배울 필요가 없습니다. **블록을 순서대로 쌓기만** 하면 됩니다.

```
┌─────────────────────────────────────────────┐
│  🔍 템플릿 매칭   ← 화면에서 이 버튼을 찾아라     │
│  🖱️ 마우스        ← 찾은 위치를 클릭해라          │
│  ⌨️ 키보드        ← 스페이스바를 눌러라           │
│  ⏱️ 딜레이        ← 0.5초 기다려라              │
└─────────────────────────────────────────────┘
             ↻  원하는 만큼 반복
```

---

## ✨ 주요 기능

| | |
|---|---|
| 🧩 **비주얼 블록 워크플로우** | 드래그로 순서를 바꾸고, 블록마다 세부 옵션을 편집. 코딩 필요 없음 |
| 🔍 **템플릿 매칭 (OpenCV)** | 화면에서 원하는 이미지를 캡처해 실시간으로 탐지. 멀티 스케일 · 다중 ROI · 다중 템플릿 지원 |
| 🎯 **정밀 화면 캡처** | DWM 확장 프레임 기준으로 대상 창을 정확히 포착. 테두리 없는 게임 창까지 대응 |
| ⌨️ **입력 시뮬레이션** | 마우스 클릭 · 휠 · 사이드 버튼, 키 입력과 조합키. 사용자가 누르고 있던 키는 건드리지 않음 |
| 🔥 **전역 단축키** | 기능마다 키보드/마우스 단축키를 지정해 즉시 실행 · 정지 (토글 · 홀드 · 반복 모드) |
| 💹 **poe.ninja 시세 계산기** | 스프레드시트 + 실시간 환율 연동 + `=A1+B1` 수식으로 아이템 시세 계산 |
| 🖥️ **똑똑한 UX** | 드래그로 값 조절하는 숫자 입력, 실행 위치 시각 피드백, 트레이 최소화, Windows 시작 시 자동 실행 |
| ⬆️ **인앱 자동 업데이트** | GitHub 릴리즈를 감지해 원클릭 설치 후 자동 재시작 |

---

## 🚀 시작하기

### 설치 (권장)

1. [**최신 릴리즈**](https://github.com/Baegovda/PIPBONG/releases/latest)에서 `PIPBONG-win64.zip`을 받습니다.
2. 압축을 풀면 `PIPBONG/` 폴더가 생깁니다.
3. 폴더 안의 **`PIPBONG.exe`** 를 실행하세요. 끝!

> 💡 인앱 **업데이트** 버튼을 누르면 이후 버전은 자동으로 받아 설치됩니다.

### 기본 사용법

1. **창 지정** — 자동화할 프로그램 창을 마우스로 콕 찍습니다. _(마우스를 올리면 대상 창 테두리가 반짝입니다 ✨)_
2. **기능 추가** — 왼쪽 목록에 새 기능(매크로)을 만듭니다.
3. **블록 조립** — 템플릿 매칭 · 마우스 · 키보드 · 딜레이 블록을 순서대로 쌓습니다.
4. **단축키 지정** — 원하면 기능마다 전역 단축키를 연결합니다.
5. **실행** — 단축키 또는 실행 메뉴로 시작. 변경 사항은 자동 저장됩니다.

---

## 🛠️ 소스로 빌드하기

<details>
<summary>개발 환경 & 빌드 명령 (클릭해서 펼치기)</summary>

<br />

**필요 사항**

- Windows 10/11
- Visual Studio 2022 (C++ 데스크톱 개발)
- CMake 3.20+
- [vcpkg](https://github.com/microsoft/vcpkg)

**빌드**

```powershell
.\scripts\build-release.ps1   # 증분 빌드 (권장 — Ctrl+Shift+B와 동일)
.\scripts\deploy-qt.ps1       # Qt/OpenCV 런타임 DLL 배포 (최초 1회)
```

첫 설정이 필요하면 `build-release.ps1`이 `build/`가 없을 때만 자동으로 `cmake --preset default`를 실행합니다.

**결과물:** `build/Release/PIPBONG.exe`

**배포용 ZIP 만들기**

```powershell
.\scripts\package-release.ps1   # dist/PIPBONG-win64.zip 생성
```

</details>

### 기술 스택

`C++17` · `Qt 6 Widgets` · `OpenCV 4` · `nlohmann/json` · `Win32 API` · `CMake` · `vcpkg`

---

## 📋 업데이트 로그

버전마다 무엇이 바뀌었는지 한글로 정리해 두었습니다.

👉 **[업데이트 로그 보기](UpdateLog/update_log.md)**

---

## 📚 문서

이 프로젝트의 모든 개발 문서(아키텍처, 빌드 정책, JSON 포맷, 변경 이력, 구현 패턴)는
단일 마스터 문서 **[`AGENTS.md`](AGENTS.md)** 에 담겨 있습니다.

---

## ⚠️ 주의

자동화 도구 사용은 대상 애플리케이션의 **이용 약관에 위배될 수 있으며**, 계정 제재로 이어질 수 있습니다.
**모든 사용에 따른 책임은 사용자 본인에게 있습니다.**

---

<div align="center">

_100% AI가 유지보수하는 프로젝트 — Made with 🤖 on Windows_

</div>
