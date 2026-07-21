# PIPBONG

Windows용 화면 자동화 프로그램입니다. 타겟을 지정한 뒤, 블록을 순서대로 쌓아 반복 작업을 실행합니다.

- 템플릿 매칭, 마우스/키보드 입력, 대기
- 기능별 전역 단축키, 프로필, 자동 저장
- [최신 릴리즈](https://github.com/Baegovda/PIPBONG/releases/latest) · [업데이트 로그](UpdateLog/update_log.md)

## 설치

1. [Releases](https://github.com/Baegovda/PIPBONG/releases/latest)에서 `PIPBONG-win64.zip` 다운로드
2. 압축 해제 후 `PIPBONG/PIPBONG.exe` 실행

앱 안 **업데이트** 메뉴로 이후 버전을 받을 수 있습니다.

## 사용

1. **타겟 지정**으로 자동화할 프로그램 선택
2. 기능을 만들고 블록(템플릿 매칭, 마우스, 키보드, 딜레이 등) 추가
3. 필요하면 단축키 지정 후 실행

## 빌드

Windows 10/11, Visual Studio 2022, CMake 3.20+, vcpkg.

```powershell
.\scripts\build-release.ps1
.\scripts\deploy-qt.ps1   # 최초 1회
```

개발·빌드 상세: [AGENTS.md](AGENTS.md)

## 주의

타겟 프로그램 이용 약관을 위반할 수 있습니다. 사용 책임은 본인에게 있습니다.
