# AGENTS.md — PIPBONG Master Document

**Current version:** `0.8.35` (from `project(PIPBONG VERSION 0.8.35)` in `CMakeLists.txt` → `PipbongVersion.h` → `QCoreApplication::applicationVersion()`)

**Repository folder:** `Sbm1.0` (local workspace path; application is **PIPBONG**)

This is the **only development document** — AI handover, user quick start, development governance, version history, and implementation patterns. Append changelog entries to [§11](#11-changelog-and-version-history) under `[Unreleased]`. Do not create other doc files. The sole exception is **`README.md`** at the repo root: a user-facing GitHub landing page (feature highlights, install, quick start) that links back here for all development detail — keep it high-level and do not duplicate handover/procedure content into it.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Stack and Dependencies](#2-stack-and-dependencies)
3. [Build and Run](#3-build-and-run)
   - [3.1 IDE / Cursor build workflow](#31-ide--cursor-build-workflow-mandatory--do-not-regress)
   - [3.6 GitHub backup and release](#36-github-backup-and-release)
4. [Repository Map](#4-repository-map)
5. [Architecture and Key Subsystems](#5-architecture-and-key-subsystems)
6. [UX Flows](#6-ux-flows)
7. [JSON and Project Format](#7-json-and-project-format)
8. [Critical Implementation Patterns](#8-critical-implementation-patterns)
9. [Development Governance](#9-development-governance)
10. [Versioning Policy](#10-versioning-policy)
11. [Changelog and Version History](#11-changelog-and-version-history)
12. [Risk and Legal Notices](#12-risk-and-legal-notices)
13. [Cursor Rules Summary](#13-cursor-rules-summary)

---

## 1. Project Overview

**PIPBONG** is a C++17 / Qt6 Windows desktop automation utility with a visual block workflow editor. It targets any application window by title — not a single game. There is **no Python** or external script runtime.

| Aspect | Detail |
|--------|--------|
| **Purpose** | Define automation “features” (macros) as ordered block workflows; capture screen templates; match images; simulate mouse/keyboard input |
| **Target OS** | Windows 10/11 only (Win32 capture, `SendInput`, DWM APIs) |
| **UI language** | Korean in-app (`tr()`, `QStringLiteral`) |
| **Doc/code language** | English |
| **Maintenance model** | 100% AI-maintained; human user directs work in Korean chat only |

**Warning:** Automation may violate target application Terms of Service. Use at your own risk.

---

## 2. Stack and Dependencies

| Layer | Technology |
|-------|------------|
| Language | C++17 |
| GUI | Qt 6 Widgets |
| Vision | OpenCV 4 (`opencv_core`, `opencv_imgproc`, `opencv_imgcodecs`) — template matching |
| Serialization | nlohmann/json |
| Platform | Win32 (`user32`, `gdi32`, `dwmapi`) — `SendInput`, GDI BitBlt, DWM extended frame bounds |
| Package manager | [vcpkg](https://github.com/microsoft/vcpkg) via `vcpkg.json` |

**vcpkg dependencies** (`vcpkg.json`):

- `qtbase`
- `opencv4`
- `nlohmann-json`

**CMake:** 3.20+, Visual Studio 2022 generator (see `CMakePresets.json`).

---

## 3. Build and Run

### Requirements

- Windows 10/11
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.20+
- vcpkg (toolchain path configured in `CMakePresets.json`)

### Configure and build (default — fast dev)

**Default for AI and daily work.** Dynamic DLL linking; **incremental** compiles (changed `.cpp` only). Dev builds **skip `windeployqt` on every link** (`PIPBONG_SKIP_WINDEPLOY=ON`) so no-change rebuilds stay ~3 s and single-file edits ~5 s.

#### First time on a machine (once)

```powershell
cmake --preset default
cmake --build build --config Release
.\scripts\deploy-qt.ps1
```

Run **`deploy-qt` once** after the first successful build (or when Qt/OpenCV DLLs beside the exe are missing). Do **not** run it after every incremental build.

#### Everyday incremental (default)

```powershell
cmake --build build --config Release
```

Or **Ctrl+Shift+B** / `빌드.bat` → `scripts/build-release.ps1` (same command; kills running exe before link).

| Situation | Typical time |
|-----------|----------------|
| One `.cpp` changed | ~5–15 s |
| No source changes | ~3 s |
| `CMakeLists.txt` / version bump (full recompile) | ~2–3 min |

Only re-run `cmake --preset default` when `build/` does not exist yet, or after changing `CMakeLists.txt` (new sources), `CMakePresets.json`, or `vcpkg.json`. CMake re-configures automatically on the next `--build` when `CMakeLists.txt` changes — **do not** run `--preset` again just for a version number change.

**Output:** `build/Release/PIPBONG.exe` — run from `build/Release/` (DLLs deployed via `deploy-qt`, not every build).

#### Deploy Qt DLLs (when needed)

```powershell
.\scripts\deploy-qt.ps1
```

Equivalent: `cmake --build build --config Release --target deploy-qt`

#### AI build policy

| When | Build? | Command |
|------|--------|---------|
| **Task close** — C++/headers/`CMakeLists.txt` changed | **Yes** — incremental only (changed `.cpp` recompile) | `cmake --build build --config Release` or `scripts/build-release.ps1` |
| Task close — docs / rules / changelog only | **No** | — |
| Mid-implementation (unless compile check needed) | **No** | — |
| User explicitly asks mid-task (e.g. “빌드해줘”) | **Yes** | same incremental command |
| `build/` missing at close | Configure once, then build | `cmake --preset default` then `--build` |
| **Version bump at task close** | **Yes** — backup + GitHub release (mandatory) | `git commit` / `git push` then `scripts/create-github-release.ps1` — see [§3.6](#36-github-backup-and-release) |
| Ad-hoc distribution only (no version bump) | Package ZIP | `scripts/package-release.ps1` |

Before link, kill a running `PIPBONG.exe` only when a build is actually run (`LNK1104`).

### Distribution — dynamic DLL ZIP

Build and ship a **folder layout** (exe + Qt/OpenCV DLLs), not a single static executable.

```powershell
.\scripts\package-release.ps1
```

**Output:** `dist/PIPBONG-win64.zip` — extract to get a **`PIPBONG/`** folder with `PIPBONG.exe`, `README.txt`, `PIPBONG 실행.bat`, Qt plugin subfolders, and runtime DLLs only (unused vcpkg modules pruned).

**Local staging folder:** `dist/PIPBONG/` (same layout as inside the ZIP).

**GitHub release (mandatory on every version bump):** After each task-close version bump, run **`.\scripts\create-github-release.ps1`** (packages + publishes ZIP to **`Baegovda/PIPBONG`**, deletes older releases). Prerequisite: commit and push source first — see [§3.6](#36-github-backup-and-release). In-app **파일 → 업데이트** downloads `PIPBONG-win64.zip` from that repo.

### Run

```powershell
.\build\Release\PIPBONG.exe
```

**One-click build (Windows):** double-click `빌드.bat` in the repo root, or in Cursor press **Ctrl+Shift+B** (default build task → `scripts/build-release.ps1`). Kills a running `PIPBONG.exe` before link when building.

**Distribution:** extract `dist/PIPBONG-win64.zip` and run `PIPBONG.exe` from that folder.

### Build pitfalls

- **LNK1104:** Kill any running `PIPBONG.exe` before linking (only when building).
- **Slow builds:** Do not run `cmake --preset default` on every task — use incremental `--build` only. A version bump regenerates `PipbongVersion.h` and recompiles only `Application.cpp` + `UpdateChecker.cpp` (not every `.cpp`).
- **Missing DLLs at runtime:** Run `.\scripts\deploy-qt.ps1` once — dev builds no longer call `windeployqt` every link.
- **vcpkg path:** `CMakePresets.json` may reference a machine-specific `CMAKE_TOOLCHAIN_FILE`; adjust if vcpkg is installed elsewhere.

### 3.1 IDE / Cursor build workflow (mandatory — do not regress)

**Status:** Verified working on Windows (2026-06). This is the **canonical daily dev path** for humans and AI. If the IDE “breaks” (CMake configure loops, vcpkg rebuilds, F5 failures, exit code -1), **restore this section** — not CMake Tools defaults.

#### What to use

| Action | Command / UI |
|--------|----------------|
| **Build** | **Ctrl+Shift+B**, `빌드.bat`, or `.\scripts\build-release.ps1` |
| **Run** | Run and Debug → **`Run PIPBONG (Release)`** → **F5** |
| **Binary** | `build/Release/PIPBONG.exe` (working directory `build/Release/`) |
| **Task close (AI)** | Same script when C++/headers/`CMakeLists.txt` changed |

`build-release.ps1` kills a running `PIPBONG.exe`, auto-runs `cmake --preset default` only if `build/CMakeCache.txt` is missing, then `cmake --build build --config Release`.

#### Required tracked `.vscode/` files (recovery source of truth)

These files **must** remain in git (see `.gitignore` whitelist). They are **not** optional local-only config.

| File | Purpose |
|------|---------|
| `.vscode/tasks.json` | Default build task **`Build Release`** → `scripts/build-release.ps1` |
| `.vscode/launch.json` | **`Run PIPBONG (Release)`** — Release exe + `preLaunchTask`: `Build Release` |
| `.vscode/settings.json` | **`cmake.enabled`: `false`** — prevents CMake Tools from hijacking F5/configure |
| `.vscode/extensions.json` | Discourage `ms-vscode.cmake-tools` and C# Dev Kit in this workspace |

**Critical setting:** `"cmake.enabled": false` in workspace `settings.json`. Without it, CMake Tools may run **configure → vcpkg → qtbase** on F5 or open (10–30 minutes) and show **“A CMake task is already running”** (exit -1).

#### Do not use for daily dev

- CMake Tools status-bar **[Configure]** / **[Build]** as the primary build path
- Launch configs with `"type": "cmake"` or `preLaunchTask: "CMake: build …"`
- Separate **`build-clangd/`** tree (IntelliSense noise; excluded in `settings.json`)
- Debug-only F5 without Release workflow unless user explicitly requests debug setup
- Placeholder `launch.json` (`<your program>`) or re-gitignoring entire `.vscode/`

#### Recovery checklist (IDE build broken)

1. `git checkout -- .vscode/` (or restore from this doc / `.cursor/rules/ide-build-workflow.mdc`).
2. Confirm `cmake.enabled` is `false` in `.vscode/settings.json`.
3. **Developer: Reload Window** in Cursor.
4. `Stop-Process -Name cmake,msbuild -Force -ErrorAction SilentlyContinue` if a task is stuck.
5. `.\scripts\build-release.ps1` — expect ~3 s (no changes) or ~5–15 s (one `.cpp`).
6. F5 with **Run PIPBONG (Release)**; if Qt DLL error, run `.\scripts\deploy-qt.ps1` once.

Cursor rule: `.cursor/rules/ide-build-workflow.mdc` (always applied).

### 3.6 GitHub backup and release

**Single repository:** **`Baegovda/PIPBONG`** — source code, git history, and GitHub Releases (ZIP) all live here. Legacy **`Baegovda/PIPBONG-releases`** is obsolete; delete it once (see below).

#### Mandatory on every version bump (task close)

Whenever an AI task closes with a **version bump** (`CMakeLists.txt` `project(PIPBONG VERSION …)` incremented per [§10](#10-versioning-policy)), **always** run backup **and** release in the same task — do not wait for the user to ask.

| Step | Action |
|------|--------|
| 1 | Finish code/docs, changelog, and version bump ([§10](#10-versioning-policy)) |
| 2 | Incremental Release build when C++/headers/`CMakeLists.txt` changed (`scripts/build-release.ps1` or `cmake --build build --config Release`); skip for docs/rules-only |
| 3 | **Backup:** `git add` (tracked sources only — not `build/`), `git commit`, `git push origin main` on **`Baegovda/PIPBONG`** |
| 4 | **Release:** `.\scripts\create-github-release.ps1` (runs `package-release.ps1`, uploads `dist/PIPBONG-win64.zip`, tag `vX.Y.Z`, deletes older releases) |

**Prerequisites:** [GitHub CLI](https://cli.github.com/) (`gh auth login` once). Remote: `https://github.com/Baegovda/PIPBONG.git`.

**Do not** finish a version-bump task with only local commits or only `[Unreleased]` changelog — push and publish the matching GitHub Release every time.

#### Ad-hoc (user phrase — same commands)

| User says (Korean) | AI action |
|--------------------|-----------|
| **백업해줘** | `git add` / `git commit` / `git push origin main` on **`Baegovda/PIPBONG`** |
| **릴리즈 해줘** | `scripts/create-github-release.ps1` only (package + publish) |
| **백업하고 릴리즈까지 해줘** | Commit + push + `create-github-release.ps1` (same as mandatory version-bump close) |

**Release script:** `scripts/create-github-release.ps1` reads version from `CMakeLists.txt`, runs `package-release.ps1`, uploads `dist/PIPBONG-win64.zip` to **`Baegovda/PIPBONG`** Releases, then deletes older releases (latest only visible).

**In-app update:** `PipbongVersion.h.in` → `PIPBONG_UPDATE_GITHUB_REPO` = `Baegovda/PIPBONG`; asset `PIPBONG-win64.zip` (`UpdateChecker.cpp`).

**Delete legacy `PIPBONG-releases` repo (one-time):** requires `delete_repo` scope — `gh auth refresh -h github.com -s delete_repo`, then `gh repo delete Baegovda/PIPBONG-releases --yes`. Or delete from GitHub **Settings → Danger zone** on that repo.

---

## 4. Repository Map

```
Sbm1.0/                        # repo root (local workspace)
├── AGENTS.md                  # this file — sole project documentation
├── CMakeLists.txt             # version source of truth, source list
├── CMakePresets.json          # VS2022 + vcpkg preset
├── vcpkg.json                 # dependency manifest
├── 빌드.bat                   # one-click Release build → scripts/build-release.ps1
├── scripts/
│   ├── build-release.ps1      # canonical incremental Release build (IDE + AI task close)
│   ├── deploy-qt.ps1          # Qt + vcpkg runtime DLLs beside build/Release
│   ├── package-release.ps1    # build + deploy + dist/PIPBONG-win64.zip
│   ├── create-github-release.ps1
│   └── fetch-poe2db-ko-names.py  # regenerate EconomyNameKoData.inc
├── .vscode/                   # tracked IDE workflow — tasks, launch, settings (see §3.1)
│   ├── tasks.json             # Ctrl+Shift+B → build-release.ps1
│   ├── launch.json            # F5 → Run PIPBONG (Release)
│   ├── settings.json          # cmake.enabled: false
│   └── extensions.json
├── .cursor/rules/             # always-applied Cursor agent rules
└── src/
    ├── main.cpp               # DPI awareness before QApplication
    ├── app/                   # Application, MainWindow, HotkeyManager
    ├── model/                 # Project, Feature domain models
    ├── core/
    │   ├── capture/           # ScreenCapture — window find, multi-strategy capture
    │   ├── vision/            # ImageMatcher — OpenCV template matching
    │   ├── input/             # InputSimulator, HotkeyBinding
    │   ├── poeninja/          # PoeNinjaClient — PoE2 economy API (calculator)
    │   └── workflow/          # Engine, blocks, execution context
    ├── storage/               # JsonSerializer — project JSON I/O
    └── ui/                    # Feature list, workflow editor, block editors, overlay
        ├── calculator/        # SpreadsheetModel, CalculatorDialog (poe.ninja 시세 계산기)
        └── editors/
            ├── ScreenRegionOverlay.*   # Win32 screen-region picker (mandatory pattern)
            ├── ImageFindEditor.*       # template capture, ROI, match test
            ├── BlockEditorDialog.*     # modal block editor host
            └── ...
```

### Path reference

| Path | Purpose |
|------|---------|
| `src/app/` | `Application`, `MainWindow`, menus, auto-save, run/stop, shutdown |
| `src/core/capture/` | `ScreenCapture` — window find, PrintWindow/BitBlt fallbacks, DPI mapping |
| `src/core/vision/` | `ImageMatcher` — `PreparedTemplate`, multi-scale matching, NMS |
| `src/core/input/` | `InputSimulator` — mouse/keyboard via Win32; `HotkeyBinding` |
| `src/core/workflow/` | `WorkflowEngine`, `Workflow`, `ExecutionContext`, block types |
| `src/core/poeninja/` | `PoeNinjaClient` — unofficial poe.ninja PoE2 Currency Exchange API |
| `src/ui/calculator/` | `CalculatorDialog`, `SpreadsheetModel` — 시세 계산기 (workflow-independent) |
| `src/ui/` | `FeatureListPanel`, `WorkflowEditorPanel`, `BlockListWidget`, editors |
| `src/ui/editors/ScreenRegionOverlay.*` | Win32 overlay for in-game template capture |
| `src/storage/` | `JsonSerializer` |
| `src/model/` | `Project`, `Feature` |

---

## 5. Architecture and Key Subsystems

### 5.1 Application layer (`src/app/`)

- **`Application`**: Qt app subclass; sets org/name/version; imports prior-install project + settings on first launch; exposes `dataDirectory()`, `autoSaveFilePath()`, `ensureDpiAwareness()`.
- **`MainWindow`**: Main UI shell — target window title, feature list, workflow editor, run control, exit/shutdown (`prepareForShutdown` stops worker, dismisses overlays, flush auto-save).
- **`HotkeyManager`**: Global hotkeys via `RegisterHotKey` / `WM_HOTKEY` on a hidden message-only Win32 host; restores previous foreground window before running workflow (no focus steal to PIPBONG).

### 5.2 Screen capture (`ScreenCapture`)

- Finds target window by title; stores `HWND`.
- **DWM extended frame bounds** for accurate borderless/windowed game window rect.
- **Multi-strategy capture:** PrintWindow, screen BitBlt, full-screen crop (GPU-rendered game fallback).
- **Search areas:** `FullScreen`, `TargetWindow`, `CustomRegion`; legacy `ScreenPercent` loads as `CustomRegion` with window-relative %.
- **Physical vs logical pixels:** `capturePhysicalRect`, `getTargetWindowScreenRect`, Win32 `PhysicalToLogicalPointForPerMonitorDPI` / `LogicalToPhysicalPointForPerMonitorDPI`.

### 5.3 Vision (`ImageMatcher`)

- `PreparedTemplate`: cached BGR + grayscale.
- `MatchOptions`: threshold (default 0.85), `multiScale` (default false), `minScale`/`maxScale` (default 0.9–1.1).
- `MatchResult`: location, center, matched size, confidence, scale.
- **Selection policy:** When multiple hits exceed threshold, choose **top-leftmost** (smallest Y, then X), not highest confidence; overlapping duplicates suppressed via NMS.
- `findAllTemplates` for match-test UI.

### 5.4 Input (`InputSimulator`, `HotkeyBinding`)

- Mouse clicks (left/right/middle, count, client vs screen coords).
- Keyboard via virtual key + modifiers (`SendInput`); modifier press/release guarded by `GetAsyncKeyState` so physically held keys are not cleared when a workflow loop ends ([§8.6](#86-physical-keyboard-state-during-workflow-runs-mandatory--do-not-regress)).
- Client-coordinate clicks on target `HWND` use `SendMessage`; feature hotkeys use low-level hooks that swallow consumed events.
- Feature hotkeys serialized under feature `hotkey` in JSON; duplicate-binding warning in UI.

### 5.5 Workflow engine

- **Block types (UI):** `ImageFind`, `Click`, `KeyPress`, `Wait`, `Text`. Workflow **구간 반복** uses `workflow.loopRegions` (not a block type).
- **`WorkflowEngine`**: Runs workflow on **`QThread` worker**; emits `started`, `blockStarted`, `blockFinished` (with ImageFind match confidence + preview pixmap), `logMessage`, `finished`.
- **`ExecutionContext`**: Stop flag, last match point/confidence/preview, logging callback.
- **ImageFind execution:** Poll loop until match, timeout, or stop; sets last match for subsequent `Click` blocks (`LastMatch` target).

### 5.6 UI

- **Profile list:** Unlimited profiles; manual select, drag reorder, edit linked target-window title per profile; **foreground-window auto-switch** (250 ms poll) activates the profile whose linked title best matches the focused top-level window (longest substring wins), falls back to the default profile when unmatched; PIPBONG foreground keeps the current profile (`MainWindow::syncProfileToForegroundWindow`, `ProfileManager::profileIdForForegroundTitle`).
- **Feature list:** Create/delete/rename features; hotkey binding (button, double-click, context menu).
- **Workflow editor:** Block list with drag-and-drop reorder; per-type **블록 추가** buttons (템플릿 매칭, 마우스, 키보드, 딜레이); template thumbnails (48×48); block editors in `BlockEditorDialog`.
- **ImageFind editor:** ROI preview, match test, screen capture overlay, `CaptureConfirmDialog`.
- **Run feedback:** ImageFind match confidence and cropped match thumbnail appear in the workflow block list row (**매칭 점수**, **매칭** columns) during execution.

### 5.7 Persistence

| Item | Location |
|------|----------|
| Active profile manifest | `%LOCALAPPDATA%/PIPBONG/PIPBONG/profiles/manifest.json` (`activeProfileId`, `defaultProfileId`, unlimited profile list) |
| Profile project file | `%LOCALAPPDATA%/PIPBONG/PIPBONG/profiles/{profileId}/project.json` via `ProfileManager` |
| Profile settings file | `%LOCALAPPDATA%/PIPBONG/PIPBONG/profiles/{profileId}/profile-settings.json` for profile-scoped execution options (`autoSelectRunningFeature`, `pinTargetWindowToScreenCenter`, `imageFindCaptureMode`, `runWithoutTargetWindow`, `linkedTargetProcessPath` for persisted target-program icon when the window is not running) |
| Templates | `%LOCALAPPDATA%/PIPBONG/PIPBONG/profiles/{profileId}/templates/*.png` |
| Manual save/open | File menu; last path in `QSettings` key `project/lastFile` |
| Debounce | 800 ms after edits; also on window close |
| Global program settings | `QSettings` — app-wide settings such as `program/launchAtWindowsStartup`, `program/closeToTray`, `program/runAsAdministrator`, `program/autoInstallUpdates`, `program/updateCheckIntervalMinutes`, and `program/pointerFeedback/click/*`; bottom **설정** button opens program settings dialog |
| Calculator sheet | `QSettings` — `calculator/sheet_v1` (JSON cell array), `calculator/lastLeague`, `calculator/geometry` |

### 5.8 poe.ninja economy calculator

- **Entry:** bottom **계산기** button (left of **설정**); modeless `CalculatorDialog`.
- **`PoeNinjaClient`:** `GET /poe2/api/data/index-state` then `GET /poe2/api/economy/exchange/{version}/overview?league={DisplayName}&type=Currency` (unofficial, no auth; `league` must be display name e.g. `Runes of Aldur`).
- **`SpreadsheetModel`:** 20×8 grid — manual numbers, `=A1+B1` formulas (`FormulaEvaluator`: `+ − * /`, parentheses, cell refs), **시세 연동** API cells (base-currency-relative values); recalculates on edit or refresh.
- **`FormulaBuilderDialog`:** **수식 만들기** — in-dialog arithmetic manual, operator buttons, operand fields, live preview; **표에서 고르기** pick mode on the grid (click or drag-select cells; Esc cancels).
- **Persistence:** `QSettings` keys `calculator/sheet_v1`, `calculator/lastLeague`, `calculator/baseCurrencyId`, `calculator/decimalPlaces` (0–8, default 4), `calculator/autoRefreshEnabled`, `calculator/autoRefreshMinutes` (1–120, default 5), `calculator/economyFavorites_v1`, `calculator/geometry` (not in `project.json`).

---

## 6. UX Flows

### Primary workflow

1. Use **창 지정** in **대상 창** to pick the target window.
2. Create a **feature** in the left panel.
3. Add blocks on the right; edit each block (ImageFind supports **화면에서 캡처** / Capture from Screen).
4. Optionally bind a **global hotkey** per feature.
5. **Run** via **실행** menu or feature hotkey — executes selected feature on worker thread.
6. Changes **auto-save**; manual File menu for export/open.

### ImageFind template capture

1. Open ImageFind block editor → **화면에서 캡처**.
2. Win32 overlay over target window; drag-select region; Esc cancels.
3. `CaptureConfirmDialog` shows preview/metadata → save to `templates/`.
4. Template path stored relative to project directory.

### Shutdown

- **종료** button (bottom-right) or window close → `prepareForShutdown`: stop worker, `ScreenRegionOverlay::dismissAll()`, flush auto-save; confirm if workflow running.

### Run controls

- Run/stop: **실행** menu and feature hotkeys (main toolbar run/stop buttons removed).
- Title bar: `PIPBONG {version}` or `PIPBONG {version} - {filename}` when project file open.

### Economy calculator (poe.ninja)

1. Click bottom **계산기** (left of **설정**).
2. Select league → **새로고침** loads PoE2 economy exchange rates.
3. Edit cells: numbers, **수식 만들기** (or type `=A1+B1`), or **시세 연동** on selected cell.
4. Sheet, base currency, favorites, and window layout persist in `QSettings` across restarts.

---

## 7. JSON and Project Format

### Root `project.json`

```json
{
  "version": 1,
  "targetWindowTitle": "",
  "projectDirectory": "C:/path/to/project/data",
  "features": [ /* ... */ ]
}
```

| Field | Description |
|-------|-------------|
| `version` | Project format version (`Project::version()`) |
| `targetWindowTitle` | Target window title for `FindWindow` |
| `projectDirectory` | Base path for relative template paths |
| `features` | Array of feature objects |

### Feature object

```json
{
  "id": "uuid-string",
  "name": "Feature name",
  "enabled": true,
  "runMode": "Toggle",
  "repeatCount": 1,
  "hotkey": { "virtualKey": 112, "ctrl": false, "alt": false, "shift": false },
  "workflow": [ /* blocks */ ]
}
```

| Field | Default | Notes |
|-------|---------|-------|
| `runMode` | `"RepeatCount"` | `Hold`, `RepeatInfinite`, `RepeatCount`, `Trigger` (legacy `"Toggle"` loads as `RepeatCount`) |
| `repeatCount` | `1` | Used when `runMode` is `RepeatCount` |
| `triggerCooldownMs` | `1000` | After a trigger fires in `Trigger` mode, wait this many ms before monitoring again (5 ms step, `0` = immediate); omitted when default |
| `infiniteExitAfterConsecutiveMisses` | `0` (omitted) | When `> 0` with `RepeatInfinite` or `Hold`, stop after this many consecutive loop iterations where template matching fails |
| `userInputInterrupt` | `"Stop"` (omitted) | `"Pause"` — toggle pause/resume on physical keyboard or mouse-button input during run; `"Stop"` — stop the run. Legacy `"None"` loads as `"Stop"`. Excludes mouse movement, injected input, and the feature's own hotkey |
| `pointerVisualFeedback` | `true` (omitted) | When `false`, disables target-window click/match pulse overlay for this feature during runs |
| `restoreMousePositionOnEnd` | `false` (omitted) | When `true`, moves the mouse cursor back to its screen position when the workflow session started |
| `lockMouseToScreenCenterDuringRun` | `false` (omitted) | When `true`, clips the physical cursor to the target window center (DWM bounds) for the feature run session; follows window moves (`MouseCenterLock`, mouse block editor) |
| `lockMouseToCurrentPositionDuringRun` | `false` (omitted) | When `true`, clips the physical cursor to its feature-start screen position for the run session (configured in mouse block editor; mutually exclusive with center lock in UI) |
| `roiCorrection` | `false` (omitted) | When `true` with **무한 반복** or **N회 반복** (≥2), applies ROI correction to **all** ImageFind blocks in the feature. When `false`, enable per block via workflow **ROI 보정** column or ImageFind block editor (`ImageFind` `roiCorrection`) |
| `roiCorrectionExpandPercent` | `110` (omitted) | Template-relative corrected search ROI size on loop 2+ when feature `roiCorrection` is on (see ImageFind `roiCorrectionExpandPercent`) |
| `editFirstTemplateRoiOnStart` | `false` (omitted) | When `true`, before the first run of a session, show editable ROI overlay on the first workflow ImageFind block that has templates and custom ROIs; **확인** saves ROI to the block and starts the run; Esc cancels the run |
| `hotkeyAllowExtraModifiers` | `false` (omitted) | When `true`, feature hotkey fires even if extra Ctrl/Alt/Shift not in the binding are held (e.g. **F4** binding still runs on **Shift+F4**); default is strict exact modifier match |

`hotkey` is optional. `virtualKey` is Win32 VK code.

### Block types (JSON `type` — always English)

#### `ImageFind`

| Field | Default | Notes |
|-------|---------|-------|
| `templates` | `[]` | Relative paths under `templates/`; multiple entries per block |
| `template` | `""` | Legacy single path; loaded when `templates` is empty; first `templates` entry is also written for backward compat |
| `templateMatchMode` | `"Any"` | `"Any"` (one hit succeeds) or `"All"` (every template must match on the same capture) |
| `templateColorMode` | `"Auto"` | `"Auto"` (analyze template), `"Grayscale"` (reject saturated color UI regions in haystack), or `"Color"` (no grayscale haystack filter); omitted when `Auto` |
| `threshold` | `0.85` | Match confidence threshold |
| `pollIntervalMs` | `200` | Delay between retries when no match (5–60000 ms, 5 ms step); block polls until success or workflow stop |
| `searchArea` | `"TargetWindow"` | `FullScreen`, `TargetWindow`, `CustomRegion`; legacy `"ScreenPercent"` loads as `CustomRegion` |
| `customRegion` | `{x,y,width,height}` | Legacy single ROI (screen pixels); migrated to `customRegions` window % on load |
| `customRegions` | `[{x,y,width,height}, …]` | **Window-relative percent** (0–100 of target DWM bounds); resolved at capture time so ROI follows window move and resize |
| `customRegionsAnchoredToTargetWindow` | `true` (omitted) | Always window %; legacy absolute-pixel and virtual-desktop `ScreenPercent` JSON auto-migrates on load |
| `percentRegion` | `{x,y,width,height}` | Legacy `ScreenPercent`; migrated to `customRegions` on load |
| `multiScale` | `false` | Written as `true` when enabled |
| `minScale` / `maxScale` | `0.9` / `1.1` | Written only when non-default |
| `roiCorrection` | `false` (omitted) | Per-block ROI correction when feature `roiCorrection` is off; loop 2+ uses session-only matched template rect from loop 1, stored as window % and re-resolved each poll |
| `roiCorrectionExpandPercent` | `110` (omitted) | Loop 2+ corrected search ROI size as % of matched template (100 = same size, 110 = 10% wider/taller per axis); feature JSON uses same field for global correction |
| `returnToPreviousImageFindOnFailure` | `false` (omitted) | On detection failure (miss limit), jump workflow execution to the previous `ImageFind` block in the list |
| `retryAfterNextActionOnFailure` | `false` (omitted) | On first detection failure: run the next block once, then retry this block; on second failure jump to the next `ImageFind` block |

#### `Click`

| Field | Default | Notes |
|-------|---------|-------|
| `target` | `"LastMatch"` | `"Fixed"`, `"LastMatch"`, or `"CurrentPosition"` |
| `x`, `y` | `0` | For `Fixed` target; offset from last match center when `lastMatchRelativeOffset` is true |
| `lastMatchRelativeOffset` | `false` | When true with `LastMatch`, click at match center plus `x`/`y` offset (client coords) |
| `button` | `"Left"` | `Left`, `Right`, `Middle`, `Back`, `Forward`, `WheelUp`, `WheelDown` |
| `action` | `"Tap"` | `Tap`, `Down` (hold), `Up` (release), `MoveOnly` (cursor move only); omitted when `Tap` |
| `count` | `1` | Click count (`Tap` only) |
| `useClientCoordinates` | `true` | Client vs screen coords |
| `ctrl`, `alt`, `shift` | `false` | Keyboard modifiers held during the click |

#### `KeyPress`

| Field | Default | Notes |
|-------|---------|-------|
| `virtualKey` | `0x20` (Space) | Omitted when `useMainKey` is false |
| `useMainKey` | `true` | When `false`, only `ctrlAction` / `altAction` / `shiftAction` run |
| `action` | `"Tap"` | Main key action; omitted when `useMainKey` is false |
| `ctrlAction` | omitted (`None`) | `None`, `Tap`, `Down`, `Up` for Ctrl |
| `altAction` | omitted (`None`) | Same for Alt |
| `shiftAction` | omitted (`None`) | Same for Shift |
| `ctrl`, `alt`, `shift` | legacy | Legacy `true` loads as `Down`; omitted on save |

#### `Wait`

| Field | Default | Notes |
|-------|---------|-------|
| `ms` | `500` | Fixed delay (0–600000 ms, 5 ms step) |
| `randomRange` | `false` | Use min/max instead |
| `minMs`, `maxMs` | `0` | Random range bounds (5 ms step) |

#### `Text`

| Field | Default | Notes |
|-------|---------|-------|
| `text` | `""` (omitted) | UTF-8 string typed into the foreground target via `SendInput` (`InputSimulator::sendText`); `\n` / `\r\n` emit Enter, `\t` emits Tab. Legacy JSON `"type": "Comment"` loads as `Text` |

#### Workflow loop regions (feature `workflow` document)

Root feature `workflow` may be a **block array** (legacy) or an **object** when loop regions are used:

```json
"workflow": {
  "blocks": [ /* block objects */ ],
  "loopRegions": [
    {
      "id": "uuid",
      "startBlock": 2,
      "endBlock": 4,
      "exitCondition": "DetectionFailed",
      "detectionMissLimit": 1
    }
  ]
}
```

| Field | Default | Notes |
|-------|---------|-------|
| `startBlock` / `endBlock` | — | **1-based inclusive** block numbers matching the workflow list `#` column; regions must not overlap |
| `exitCondition` | `"DetectionFailed"` | `DetectionFailed`, `DetectionSucceeded`, `LastMatchSuccess`, `LastMatchFailed` |
| `detectionMissLimit` | `1` | Consecutive ImageFind poll misses inside the region before detection counts as failed (`DetectionFailed` only); omitted when default |

When `loopRegions` is empty, `workflow` saves as a plain block array for backward compatibility. `WorkflowRunner` repeats each region until its exit condition is met, then continues with the next block after `endBlock`.

**Stability rule:** Never localize JSON `type` values or enum strings used in serialization.

---

## 8. Critical Implementation Patterns

### 8.1 Win32 screen-region overlay (mandatory — do not regress)

**Status:** Verified working on Windows (2026-06). **Do not** replace with a Qt `QWidget` overlay without explicit user approval and full regression testing.

**Purpose:** Drag-select a template region over the POE game window from **화면에서 캡처** in `ImageFindEditor`.

#### Why not Qt `QWidget`?

Mixing Qt logical geometry, `WA_TranslucentBackground`, and `SetWindowPos` in physical pixels caused wrong overlay placement, click-through to the game (Windows error beep), and unreliable Esc handling.

#### Architecture

| Layer | Responsibility |
|-------|----------------|
| `ScreenRegionOverlay` | Static API only — **not** a `QWidget` subclass |
| Win32 popup `HWND` | Topmost layered window over POE bounds; owns input during pick |
| `ScreenCapture::getTargetWindowScreenRect()` | POE bounds via DWM `DWMWA_EXTENDED_FRAME_BOUNDS` (physical pixels) |
| `ImageFindEditor` callback | BitBlt via `ScreenCapture::capturePhysicalRect` **before** host restore |
| `MainWindow::prepareForShutdown` | Calls `ScreenRegionOverlay::dismissAll()` |

#### Win32 overlay rules

1. **Create** with `CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, …, WS_POPUP, x, y, w, h, …)` using **physical** bounds from `getTargetWindowScreenRect()`.
2. **Do not** mix Qt `setGeometry()` / `SetWindowPos()` on a Qt widget for overlay placement.
3. **Hit testing:** `SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA)` on `WM_CREATE`. **Never** `WS_EX_TRANSPARENT`.
4. **Input:** `WM_LBUTTONDOWN` / `WM_MOUSEMOVE` / `WM_LBUTTONUP` with `SetCapture` / `ReleaseCapture`. Selection in **physical** screen coordinates (`ClientToScreen`).
5. **Esc cancel:** `RegisterHotKey(hwnd, id, 0, VK_ESCAPE)` + `WM_KEYDOWN`.
6. **Paint:** GDI in `WM_PAINT` (dim fill + selection rect). Hint text from `tr()` as `std::wstring` at pick start.
7. **Teardown:** `UnregisterHotKey`, `DestroyWindow`, invoke pick callback **immediately** (BitBlt capture must see the game, not the overlay), then defer host restore via `QTimer::singleShot(0, qApp, …)`.

#### Host dialog during pick (`BlockEditorDialog`)

- Opened with `exec()`. **`hide()` or `setVisible(false)` terminates `exec()` with `Rejected`** — do not use.
- Use **`parkHostOffScreen()`** / **`restoreHost()`** instead.
- Run pick **callback before `restoreHost()`** so BitBlt captures the game, not the editor.

#### DPI

- `Application::ensureDpiAwareness()` in `main()` **before** `QApplication` (`PER_MONITOR_AWARE_V2`).
- Overlay placement stays in Win32 physical pixels.

#### Entry points

- `ScreenRegionOverlay::startPick(hostWidget, callback)` — from `ImageFindEditor`
- `ScreenRegionOverlay::dismissAll()` — shutdown, cancel, or before new pick

#### Anti-patterns (known failures)

- Qt frameless translucent `QWidget` + `WA_TranslucentBackground` for game overlay
- `grabMouse()` / `grabKeyboard()` on Qt widget instead of native mouse messages
- `WM_NCHITTEST` / `HTCLIENT` hacks on Qt layered widgets without coordinate sync
- Restoring host window before BitBlt in pick callback
- Replacing `parkHostOffScreen` with `hide()`

#### Known limitation

If POE runs **as administrator**, a non-elevated PIPBONG process may not receive input over the game window. Fix: run PIPBONG with matching elevation.

#### Key files

- `src/ui/editors/ScreenRegionOverlay.h` / `.cpp`
- `src/ui/editors/ImageFindEditor.cpp`
- `src/core/capture/ScreenCapture.cpp`
- `src/main.cpp`
- `src/app/MainWindow.cpp`

### 8.2 Workflow block list drag-and-drop

- Defer `blockRowsReordered` until after `QTableWidget::startDrag` returns.
- Disable `Qt::ItemIsDropEnabled` on row items so drops target between rows.
- Disable reorder while workflow is running.

### 8.3 Code conventions

- Match surrounding C++/Qt style; minimal diffs.
- UI strings: Korean via `tr()` / `QStringLiteral`.
- Block log messages: Korean.
- No hardcoded version strings — use `PIPBONG_VERSION` / `QCoreApplication::applicationVersion()`.

### 8.4 Qt stylesheet and changeEvent (mandatory — no startup crash)

**Never** call `setStyleSheet()` from `changeEvent` on `QEvent::StyleChange`. Qt re-posts `StyleChange` after every `setStyleSheet`, causing infinite recursion and stack overflow (`0xC00000FD`) — the app exits after a few seconds with no visible window.

| Rule | Detail |
|------|--------|
| Stylesheet | Apply **once** in constructor / `setupUi`; use `palette(...)` in QSS for theme-aware colors |
| `changeEvent` | React only to `PaletteChange` / `ApplicationPaletteChange` when updating colors |
| Dynamic colors | Use `QPalette` on labels (`setPalette`, `setStyleSheet` on children avoided) with `m_updatingTheme` reentrancy guard |
| Never | `StyleChange` → `setStyleSheet` / unguarded `setPalette` cascade |

Reference implementations: `TargetWindowDetailPanel`, `CustomTitleBar`. Cursor rule: `.cursor/rules/qt-changeevent-stylesheet.mdc`.

### 8.5 Template capture and post-pick UX (mandatory — manual verify)

After overlay pick or template capture changes, **manually test on Windows** before marking the task done:

| Check | Expected |
|-------|----------|
| Confirm dialog position | Near release cursor, clamped on-screen (`positionDialogNearGlobalPoint`, `deferUntilHostRestored`) |
| Cursor visibility | Pointer visible over confirm dialog, main window, and block editor after pick |
| PNG content | No overlay dim, no selection chrome, no cursor in saved template |
| Teardown order | Overlay destroyed → sync BitBlt (`capturePhysicalRectForTemplate`) → restore parked host → modal UI |

**Never** use `ShowCursor(FALSE)` loops in `capturePhysicalRectForTemplate` — corrupts global display count and hides the cursor over the app.

Post-pick modals: `ScreenRegionOverlay::deferUntilHostRestored` (not a bare `QTimer::singleShot` that races host restore).

Cursor rule: `.cursor/rules/ux-regression-checklist.mdc`.

### 8.6 Physical keyboard state during workflow runs (mandatory — do not regress)

**Status:** Verified working on Windows (2026-06). Ending a workflow loop must **not** release Shift/Ctrl/Alt the user was already holding **before the feature session started**. Keys PIPBONG pressed during the run (KeyPress **누름**, modifier **누름**, Click block modifiers left down) are **released automatically** when each loop iteration or the feature session ends.

#### Symptom (user-held modifiers — do not regress)

User holds **Shift** (run/walk in games) while a feature runs — often **click-only** blocks, **Hold** run mode, mouse side-button hotkey (**앞으로 가기** / XButton2), or infinite repeat. When the loop stops, the game behaves as if Shift was released even though the finger is still on the key. Workflow blocks may not include any KeyPress or modifier Click blocks.

#### Run-end keyboard restore (default)

| Rule | Detail |
|------|--------|
| Track | `InputSimulator` records synthetic KEYDOWN/KEYUP into `ExecutionContext::m_pipbongHeldVirtualKeys` while the worker runs |
| Restore | `ExecutionContext::restoreRunKeyboard()` after each `WorkflowRunner::run` and `endRunKeyboardSession()` in `finishRunSession` |
| Scope | Only virtual keys PIPBONG actually sent DOWN and did not later UP — not keys the user held before the session (those never get a synthetic DOWN) |
| Per-block guards unchanged | `GetAsyncKeyState` + pre-block `ModifierSnapshot` still prevent duplicate DOWN/UP during blocks |

#### Root causes (historical bugs — user-held modifiers)

| Cause | Why it breaks physical holds |
|-------|------------------------------|
| `AttachThreadInput` in focus restore | Detach clears modifier state Windows tracks for keys still held physically |
| Unconditional `SendInput` KEYUP | Synthetic KEYUP cancels a real hold, not only PIPBONG's own synthetic DOWN |
| `reassertPhysicallyHeldModifiers` on run finish | End-of-loop KEYDOWN/KEYUP “sync” disturbed real keyboard state — **removed** |
| Hold hotkey UP delivered to game | Feature binding release (e.g. side button UP) reaches target app when loop ends |
| Global `SendInput` for client clicks | Keyboard/mouse input queue pollution vs. in-window `SendMessage` |

#### Architecture (required)

| Layer | Responsibility |
|-------|----------------|
| `restoreForegroundWindow` | `AllowSetForegroundWindow(ASFW_ANY)` + `SetForegroundWindow` only — **never** `AttachThreadInput` (`HotkeyManager.cpp`, `ScreenCapture.cpp`) |
| `InputSimulator` | Modifier KEYDOWN/KEYUP guarded by `GetAsyncKeyState` (generic + L/R VK); track `AppliedKeyModifiers` per block; track session `m_pipbongHeldVirtualKeys` for run-end restore |
| `HotkeyManager` | Low-level keyboard/mouse hooks swallow consumed feature hotkeys (`return 1`); ignore `LLKHF_INJECTED` / `LLMHF_INJECTED` |
| Run teardown | `WorkflowEngine` worker calls `restoreRunKeyboard()` after each loop; `finishRunSession` calls `endRunKeyboardSession()` — releases **PIPBONG-held keys only**, never blind sync of full keyboard state |

#### InputSimulator rules

1. **`pressModifiersIfNeeded`**: Send KEYDOWN for Ctrl/Alt/Shift only when the Click/KeyPress block requests it **and** `GetAsyncKeyState` reports the modifier is **not** already down (check `VK_SHIFT` / `VK_LSHIFT` / `VK_RSHIFT`, same pattern for Ctrl and Alt).
2. **`releaseAppliedModifiers`**: After Tap actions, send KEYUP only for modifiers in `AppliedKeyModifiers` that were **not** already down in a pre-block `ModifierSnapshot` — never KEYUP keys the user held before the block started.
3. **`sendKey` (KeyPress block)**: For modifier virtual keys, skip Tap entirely when `isModifierVirtualKeyPhysicallyDown`.
4. **`clickAtClient`**: Convert client coords to screen, then `SetCursorPos` plus `SendInput` button down/up (no `SendMessage` clicks — games swallow those). Skip modifier snapshot when the block requests no Ctrl/Alt/Shift (`InputSimulator::clickAt`).
5. **Screen-coordinate fallback** (`clickAt` without valid client path): still uses `SendInput` for mouse; modifier press/release follows the same `AppliedKeyModifiers` guards.

#### HotkeyManager rules

1. **`holdKeyboardHookProc` / `mouseHookProc`**: When `handleHoldKeyEvent` / `handleMouseButtonEvent` returns true, return **`1`** so the event is **not** forwarded to other apps (`CallNextHookEx` must not run for swallowed events).
2. **Injected input**: Ignore events with `LLKHF_INJECTED` (keyboard) or `LLMHF_INJECTED` (mouse) so PIPBONG's own `SendInput` does not re-enter hotkey handling.
3. **Hold mode bindings**: Swallow both DOWN and UP for the bound key or mouse button so the target app never sees the feature hotkey as a click/keypress when starting or stopping the loop.
4. **Foreground**: Before emitting hold/trigger signals, restore previous foreground with the same `restoreForegroundWindow` helper (no `AttachThreadInput`).
5. **Modifier match**: Default strict — `HotkeyBinding::modifiersMatch(false)` requires Ctrl/Alt/Shift to match the binding exactly (e.g. plain **F4** does not fire on **Alt+F4**). Per-feature `hotkeyAllowExtraModifiers` opts in to loose match: required modifiers must be down; extra held modifiers are ignored (`Feature`, `FeatureEditDialog`, `HotkeyManager`).

#### Explicit removals (do not reintroduce)

- `reassertPhysicallyHeldModifiers` (was in `InputSimulator`, `HotkeyManager`, `MainWindow::finishRunSession`, `WorkflowEngine` — deleted)
- `AttachThreadInput` / `DetachThreadInput` in any focus-restore path
- Blind run-end keyboard sync (KEYUP every modifier regardless of who pressed it)

#### Manual verification (required for input / hotkey / workflow changes)

| Step | Expected |
|------|----------|
| Hold **Shift** in target app; run click-only feature on **Hold** + side-button hotkey; release hotkey | Shift remains held in-game |
| Stop infinite / N-repeat mid-loop while Shift held | Same — no modifier drop |
| Click block with **Shift** checked while user already holds Shift | No spurious release when loop ends |
| KeyPress **누름** (or modifier **누름**) during run; stop feature | SBM-pressed key is released in target app |

#### Key files

- `src/core/input/InputSimulator.cpp` — modifier guards, session key tracking, `restoreTrackedKeyboard`
- `src/core/workflow/ExecutionContext.cpp` — `restoreRunKeyboard`, `endRunKeyboardSession`
- `src/core/workflow/WorkflowEngine.cpp` — restore after each loop iteration
- `src/app/HotkeyManager.cpp` — hooks, swallow, `restoreForegroundWindow`
- `src/core/capture/ScreenCapture.cpp` — shared `restoreForegroundWindow`
- `src/app/MainWindow.cpp` — `finishRunSession` → `endRunKeyboardSession`

Cursor rule: `.cursor/rules/physical-keyboard-preservation.mdc`.

### 8.7 Drag-adjust numeric input (mandatory default)

**Status:** Program-wide policy (2026-06). Numeric fields in block editors and feature dialogs use **`DragAdjustSpinBox`** (integer) or **`DragAdjustDoubleSpinBox`** (decimal) — not stock `QSpinBox` / `QDoubleSpinBox` with up/down arrows.

| Rule | Detail |
|------|--------|
| Widgets | `src/ui/widgets/DragAdjustSpinBox.*`, `DragAdjustDoubleSpinBox.*` |
| Interaction | Single click → text input; horizontal drag anywhere on field; `NoButtons`; `SizeHorCursor` (I-beam while editing) |
| Step | Every 4 horizontal px: `singleStep()` (int and double); Shift ×10; Ctrl ×100 |
| Drag threshold | `QApplication::startDragDistance()` before drag steals the click |
| Labels | Put unit suffixes (`ms`, `회`) on adjacent labels when possible (`WaitEditor::makeMsInputRow`) |
| Never | Add raw spin boxes in UI unless user explicitly requests stock arrows |

Cursor rule: `.cursor/rules/drag-adjust-numeric-input.mdc`.

### 8.8 IDE / Cursor build workflow (mandatory — do not regress)

**Status:** Verified working on Windows (2026-06). IDE build and F5 **must not** trigger CMake Tools configure or vcpkg reinstall. Use tracked `.vscode/` + `scripts/build-release.ps1` only.

| Layer | Rule |
|-------|------|
| Build | **Ctrl+Shift+B** / `빌드.bat` / `build-release.ps1` → `cmake --build build --config Release` |
| Run | `launch.json` **`Run PIPBONG (Release)`** with `preLaunchTask` **`Build Release`** |
| CMake Tools | **`cmake.enabled`: `false`** in `.vscode/settings.json` — workspace only |
| Git | Four `.vscode/*` files whitelisted in `.gitignore` — never delete from repo |
| Exclude | `build-clangd/` — do not use as CMake build dir |

**Recovery:** [§3.1](#31-ide--cursor-build-workflow-mandatory--do-not-regress) checklist + `.cursor/rules/ide-build-workflow.mdc`.

---

## 9. Development Governance

### Ownership

- Codebase is **100% AI-maintained**: implement, refactor, document, changelog via AI.
- Human user directs work in **Korean chat only**; does not hand-edit unless coordinating with AI.

### Language policy

| Artifact | Language |
|----------|----------|
| Code, comments, docs, rules, changelog | English |
| User chat replies | Korean |
| In-app UI strings | Korean (`tr()`, `QStringLiteral`) |
| JSON block `type` and enum strings | English (serialization stability) |

### After every completed task

1. Append entries under `[Unreleased]` in [§11 Changelog](#11-changelog-and-version-history) (`Added` / `Changed` / `Fixed` / `Removed`) as you implement.
2. **Before closing the task:** bump version per [§10](#10-versioning-policy) — update `CMakeLists.txt`, move `[Unreleased]` into `## [x.y.z] - YYYY-MM-DD`, leave empty `[Unreleased]`. Then run **incremental** `cmake --build build --config Release` (or `scripts/build-release.ps1`) when C++/headers/`CMakeLists.txt` sources changed; skip build for docs/rules-only. **Then mandatory backup + GitHub release** per [§3.6](#36-github-backup-and-release): `git commit` / `git push origin main` and `scripts/create-github-release.ps1`. **Never** finish with changelog-only `[Unreleased]` entries and the same version number.
3. Keep diffs minimal; match existing C++ / Qt conventions.
4. For overlay/capture/modal UI work: run the [§8.5 template capture checklist](#85-template-capture-and-post-pick-ux-mandatory--manual-verify) on Windows before closing the task.
5. **Do not regress IDE build workflow** ([§3.1](#31-ide--cursor-build-workflow-mandatory--do-not-regress)): keep `.vscode/` tracked files and `cmake.enabled: false`; never replace F5 with CMake Tools configure-on-open.

### Diff conventions

- Focused changes only; no drive-by refactors.
- Reuse existing abstractions (`ScreenCapture`, `ImageMatcher`, `BlockFactory`, etc.).
- Comments only for non-obvious logic.

---

## 10. Versioning Policy

**Single source of truth:** `project(PIPBONG VERSION x.y.z)` in `CMakeLists.txt` → `configure_file` → `build/generated/PipbongVersion.h` → `Application::setApplicationVersion()`.

**Do not** hardcode version strings elsewhere.

### Mandatory bump every completed user task

When the user’s request is **done** (code merged, changelog written, version bumped):

| Step | Action |
|------|--------|
| 1 | Increment version in `CMakeLists.txt` (see table below) |
| 2 | Move all `[Unreleased]` bullets into `## [x.y.z] - YYYY-MM-DD` in [§11](#11-changelog-and-version-history) |
| 3 | Leave empty `[Unreleased]` (`### Added` / `Changed` / `Fixed` / `Removed` headers only, or blank) |
| 4 | Update **Current version** at the top of this file |
| 5 | **Incremental build at task close** when compile inputs changed — `cmake --build build --config Release` (or `scripts/build-release.ps1`; `--preset default` only if `build/` missing). Skip docs/rules-only. |
| 6 | **Backup:** `git add` / `git commit` / `git push origin main` on **`Baegovda/PIPBONG`** ([§3.6](#36-github-backup-and-release)) |
| 7 | **Release:** `.\scripts\create-github-release.ps1` (package + publish `vX.Y.Z` ZIP) — **mandatory on every version bump** |

**Do not** accumulate many tasks under `[Unreleased]` without bumping. **Do not** finish a chat task with changelog entries still unreleased and the same version number. **Do not** bump version without push + GitHub Release in the same task.

### Which segment to increment

| Bump | When |
|------|------|
| **Patch** `0.3.0 → 0.3.1` | **Default — every completed user task** (bugfixes, UX, docs, small features, rule updates) |
| **Minor** `0.3.x → 0.4.0` | New block type, major subsystem, or many related features in one milestone the user treats as a release |
| **Major** `1.0.0` | Breaking JSON/project format or stable release milestone |

When in doubt, **patch bump**.

### When bumping (checklist)

1. Update `project(PIPBONG VERSION ...)` in `CMakeLists.txt`.
2. Move `[Unreleased]` items into `## [x.y.z] - YYYY-MM-DD` in [§11](#11-changelog-and-version-history).
3. Leave empty `[Unreleased]` section.
4. **Incremental build at task close** when C++/headers/`CMakeLists.txt` changed: `cmake --build build --config Release` (kill running exe before link). Skip docs/rules-only.
5. **Backup + release** (mandatory): `git commit` / `git push origin main`, then `scripts/create-github-release.ps1` ([§3.6](#36-github-backup-and-release)).

### Changelog format

Follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Use English; be specific (file/behavior). **All changelog entries live in §11 of this file only.**

---

## 11. Changelog and Version History

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versioning follows [§10](#10-versioning-policy).

## [Unreleased]

### Added

### Changed

### Fixed

### Removed

## [0.8.35] - 2026-07-09

### Added

- Feature library drawer: collapsible **라이브러리 (N)** section at the bottom of the feature list panel lists saved library entries inline; double-click or context menu **현재 프로필로 가져오기** imports an entry, context menu **라이브러리에서 삭제** removes it (with confirmation); expand state persists in `QSettings` (`FeatureListPanel`, `FeatureLibraryManager::removeEntry`, `MainWindow::refreshFeatureLibraryPanel`).

## [0.8.34] - 2026-07-09

### Changed

- Per-profile target-window binding persists across restarts and when the linked program is not running: `linkedTargetProcessPath` in `profile-settings.json` keeps the program icon in the profile list; the target-window panel shows saved title/process with **● 미실행** instead of a blank pick prompt; window pick updates in-memory profile binding immediately (`ProfileManager`, `MainWindow`, `TargetWindowDetailPanel`).

## [0.8.33] - 2026-07-09

### Changed

- Global default profile: target-window panel shows a **전역 기본 프로필** info card with **시스템 고정** badge instead of an empty pick prompt; **창 지정** / **창 목록** / **창 표시** / **화면 중앙 고정** are disabled with an explanatory tooltip; profile list suffix updated to **(전역 기본)** (`TargetWindowDetailPanel`, `MainWindow`).

## [0.8.32] - 2026-07-08

### Fixed

- 기능 목록이 Windows 리스트처럼 `Ctrl+클릭`/`Shift+클릭` 다중 선택을 지원하도록 `ExtendedSelection`으로 바뀌었고, 다중 선택 상태에서 `Delete`는 선택된 여러 기능을 한 번에 삭제합니다. 다중 선택일 때는 드래그 재정렬을 막아 기존 단일행 재정렬 UX와 충돌하지 않도록 했습니다 (`FeatureListWidget`, `FeatureListPanel`).

## [0.8.31] - 2026-07-08

### Changed

- 대상 창 패널(프로세스/클래스/모니터 정보) 기본 표시를 더 컴팩트하게 줄이기 위해 3번째(모니터) 줄을 기본 접힘 처리하고, 글자/여백/버튼 높이를 축소했습니다 (`TargetWindowDetailPanel`, `MainWindow`).

## [0.8.30] - 2026-07-08

### Added

- Global feature library: save a feature from any profile into `featureLibrary/` (including referenced ImageFind template PNGs) and import it into other profiles via a picker dialog (`FeatureLibraryManager`, `FeatureLibraryDialog`, `FeatureListPanel`, `MainWindow`).

## [0.8.29] - 2026-07-08

### Changed

- Main window target-window panel layout refreshed for clarity: action buttons are grouped into a cleaner top command row, the center-pin option is separated into a secondary row, and the target info card now uses a stronger grouped surface (`MainWindow`).
- `TargetWindowDetailPanel` now presents the active window in a more readable card layout with a prominent title, state badge, and wrapped metadata lines for process, HWND, bounds, client size, and monitor details (`TargetWindowDetailPanel`).

## [0.8.28] - 2026-07-08

### Changed

- Profile auto-switch no longer re-reads each profile `project.json` on every 250 ms foreground poll, and profile target-window bindings are cached in memory after load/update (`ProfileManager`).
- Active-profile switching no longer rebuilds the entire profile list on every switch; the current row is updated in place instead, and profile-list icon resolution enumerates desktop windows only once per full list refresh (`MainWindow`).

## [0.8.27] - 2026-07-08

### Added

- Profile auto-switch: polls the foreground top-level window every 250 ms and switches to the profile whose linked target-window title best matches (longest substring wins); unmatched windows fall back to the default profile; PIPBONG foreground keeps the current profile (`ProfileManager::profileIdForForegroundTitle`, `MainWindow::syncProfileToForegroundWindow`).

## [0.8.26] - 2026-07-08

### Changed

- Default profile always runs without a designated target window: `targetWindowTitle` is cleared and `runWithoutTargetWindow` is enforced on startup, when setting default, and when creating the initial profile; profile edit hides the linked-program section for the default profile (`ProfileManager`, `ProfileEditDialog`, `MainWindow`).

## [0.8.25] - 2026-07-08

### Changed

- Profile row drag is now more direct: pressing a profile row immediately makes it the active drag candidate, so users can grab-and-drag in one motion without first pre-selecting the row (`ProfileListWidget`, `MainWindow`).

## [0.8.24] - 2026-07-08

### Changed

- Setting a profile as **default** now forces global/no-window behavior for that profile: clears `targetWindowTitle` and turns on profile-scoped `runWithoutTargetWindow`, so the default profile runs across windows without requiring target-window designation (`MainWindow`, `ProfileManager`, `ProgramSettings`).

## [0.8.23] - 2026-07-08

### Added

- Profile list supports drag-and-drop reordering; dropped order is persisted to `profiles/manifest.json` so profile order remains stable across restarts (`MainWindow`, `ProfileManager`).

## [0.8.22] - 2026-07-08

### Changed

- Double-clicking a profile row now opens the same profile edit dialog as the **편집** button for faster profile metadata edits (`MainWindow`, profile list signal wiring).

## [0.8.21] - 2026-07-08

### Added

- Profile edit dialog: rename a profile, set its linked target program title, and choose exactly one user-selected default profile that opens on startup (`ProfileEditDialog`, `ProfileManager`, `MainWindow`).

### Changed

- Profile list now labels the default profile as **(기본)** and includes default-profile status in the item tooltip (`MainWindow`).

## [0.8.20] - 2026-07-08

### Changed

- Feature hotkey execution is suppressed while PIPBONG itself is foreground-focused; additionally, opening **프로그램 설정** now holds a `FeatureHotkeyGateScope` so hotkeys cannot start features while settings are being edited (`MainWindow`, `FeatureHotkeyGate`).

## [0.8.19] - 2026-07-08

### Changed

- Profile panel border cleanup: removed the `QGroupBox` outer frame around the profile list area and kept the list background transparent/borderless so no white outline remains (`MainWindow`, `profileListGroup`, `profileListView` style).

## [0.8.18] - 2026-07-08

### Changed

- Profile panel button label updated from **이름** to **편집** for clearer intent when renaming profile metadata (`MainWindow`).

## [0.8.17] - 2026-07-08

### Changed

- Profile list visual chrome: removed the default white list frame/border and kept a clean borderless profile list surface in the profile panel (`MainWindow`, `profileListView` style).

## [0.8.16] - 2026-07-08

### Changed

- Profile panel can be resized narrower: profile action buttons are stacked vertically with width-ignored sizing, and the profile column minimum width/initial splitter width were reduced so users can shrink the profile pane further (`MainWindow`).

## [0.8.15] - 2026-07-08

### Added

- Profile system: left-side profile list next to the feature list; only the active profile's features and hotkeys run, with unlimited profiles stored under `profiles/{profileId}/project.json` and live target-program icons when the target window is present (`ProfileManager`, `MainWindow`).
- Profile-scoped execution options saved in `profile-settings.json`: auto-select running feature, target-window center pin, ImageFind capture mode, and run-without-target-window (`ProgramSettings::ProfileSettings`, `ProfileManager`, `ProgramSettingsDialog`).

### Changed

- Legacy single auto-save project migrates into the default profile on first profile-system startup; active profile switches stop running sessions, save current project/settings, load the selected profile, and resync hotkeys (`ProfileManager`, `MainWindow`).

## [0.8.14] - 2026-07-08

### Added

- Per-feature **사용** toggle in the feature list (checkbox column): disables hotkeys, run button, and execution for that feature; persisted as feature JSON `enabled` (`FeatureListPanel`, `FeatureEditDialog`, `MainWindow`, `HotkeyManager`).

### Changed

- Disabling a feature while it is running stops the active session immediately (`FeatureListPanel::featureEnabledChanged`, `MainWindow::onFeatureEnabledChanged`).

## [0.8.13] - 2026-07-08

### Changed

- Program settings dialog: checkbox option descriptions shown as inline `HintLabel` text below each option instead of hover tooltips (`ProgramSettingsDialog`).

## [0.8.12] - 2026-07-08

### Added

- Program settings **창을 지정하지 않은 상태에서도 동작**: when enabled, feature runs proceed without **창 지정**; ImageFind **대상 창** search area falls back to full screen when no target HWND is available (`program/runWithoutTargetWindow`, `ProgramSettings`, `ProgramSettingsDialog`, `ScreenCapture`, `MainWindow`, `ImageFindBlock`).

### Changed

- Feature run start now requires a resolvable target window unless **창을 지정하지 않은 상태에서도 동작** is enabled (`MainWindow::startFeatureRun`, `ScreenCapture::hasResolvableTargetWindow`).

## [0.8.11] - 2026-07-08

### Fixed

- Concurrent hold hotkeys (e.g. **Q** + **W**): `UserInputInterruptMonitor` now treats **any registered project feature hotkey** as non-interrupting input (not only already-running sessions), so pressing the second hold key no longer stops the first feature before its session starts (`UserInputInterruptMonitor`, `HotkeyManager::matchesAnyRegisteredFeatureHotkey`, `MainWindow`).
- Hold hotkey release ignores spurious key-up events while `GetAsyncKeyState` still reports the key down (keyboard rollover when pressing adjacent hold keys together), so the first hold feature no longer receives `hotkeyHoldEnded` when a second hold key is pressed (`HotkeyManager`).

## [0.8.10] - 2026-07-08

### Fixed

- Holding two feature hotkeys at once (e.g. **Q** and **W** both in **누를 동안** mode) no longer stops the first feature when the second key is pressed: `UserInputInterruptMonitor::notifyPhysicalInput` now ignores physical input that matches the hotkey of **any** currently running feature, not only the owning session, so concurrent hold features keep running independently (`UserInputInterruptMonitor`).

## [0.8.9] - 2026-07-08

### Changed

- Multiple features can run concurrently: each feature keeps its own `FeatureRunSession` / `WorkflowEngine` worker; hold hotkeys for different features start and stop independently without blocking one another (`MainWindow`, `HotkeyManager` integration unchanged).
- Mouse-center / position lock is reconciled across all active sessions when any session ends or trigger preemption pauses others, so one feature stopping no longer clears another feature's clip (`MainWindow::reconcileMouseLocksFromRunningSessions`).

### Fixed

- Stale or winding-down run sessions no longer block starting the same feature again or a second hold hotkey while loop iterations are between runs (`MainWindow::isFeatureSessionActive`, `startFeatureRun`, `onHotkeyHoldStarted`).

## [0.8.8] - 2026-07-08

### Changed

- Program settings **템플릿 매칭 캡처** group: beginner-friendly combo labels (**기본 — 화면 그대로** / **창 안만 — 커서·표시 제외**) and live `HintLabel` description per selection (`ProgramSettingsDialog`).

## [0.8.7] - 2026-07-07

### Added

- Program settings option **템플릿 매칭 캡처 방식**: choose between **기본(화면 우선, 호환성 높음)** and **클라이언트 창만 캡처(순수 캡처 우선)**; persisted in `program/imageFindCaptureMode` (`ProgramSettings`, `ProgramSettingsDialog`).

### Changed

- `ScreenCapture::captureSearchAreaForImageFind(TargetWindow)` now respects `program/imageFindCaptureMode`: default `Hybrid` keeps current screen BitBlt-first behavior, while `ClientOnly` prioritizes target client-window capture (`ScreenCapture`).

## [0.8.6] - 2026-07-06

### Added

- Title bar **도움말** menu with **PIPBONG 도움말** (quick-start sections) and **PIPBONG 정보** About dialog (`AppHelpDialog`, `MainWindow`).

## [0.8.5] - 2026-07-05

### Added

- **대상 창** panel **화면 중앙 고정** checkbox: keeps the designated target window centered on its current monitor (200 ms sync, DWM bounds, `program/pinTargetWindowToScreenCenter`, `TargetWindowCenterPin`, `MainWindow`).

## [0.8.4] - 2026-07-05

### Added

- Feature **기능 편집** option **다른 조합키와 함께 눌러도 실행**: per-feature opt-in loose hotkey modifier match (`hotkeyAllowExtraModifiers` JSON, `Feature`, `FeatureEditDialog`, `HotkeyBinding::modifiersMatch`, `HotkeyManager`, `JsonSerializer`).

## [0.8.3] - 2026-07-05

### Fixed

- **기능 편집** hotkey capture (`입력 대기 중...`) no longer triggers other features bound to the same key: `FeatureHotkeyGateScope` is held for the capture session (`FeatureEditDialog`, `HotkeyCaptureDialog`, `FeatureHotkeyGate`).

## [0.8.2] - 2026-07-04

### Changed

- **텍스트** block now types the configured string into the target app at run time (`InputSimulator::sendText`, Enter/Tab for newlines and tabs) instead of acting as a workflow memo/log-only note; editor copy updated (`TextBlock`, `TextEditor`).

## [0.8.1] - 2026-07-04

### Changed

- User-facing key labels use **Enter** instead of Qt's **Return** (feature hotkeys, keyboard block editor/summary, workflow key icons); text block hint mentions **Enter** for line breaks (`HotkeyBinding`, `KeyPressEditor`, `TextEditor`, `HotkeyBindingIcon`, `WorkflowEditorPanel`).

## [0.8.0] - 2026-07-04

### Added

- Workflow **텍스트** block (`Text` JSON): multi-line text editor; at run time types the configured UTF-8 string into the target app; legacy `"Comment"` blocks load as `Text` (`TextBlock`, `TextEditor`, `BlockFactory`, `BlockEditorDialog`, `WorkflowEditorPanel`).

## [0.7.81] - 2026-07-04

### Changed

- **기능 추가** opens **기능 편집** with an empty name field (placeholder **기능 이름** only) and keyboard focus so typing starts immediately; cancel removes the unsaved new feature (`FeatureListPanel`, `FeatureEditDialog`).

## [0.7.80] - 2026-07-04

### Fixed

- Feature hotkeys no longer fire when extra Ctrl/Alt/Shift modifiers are held: `HotkeyBinding::modifiersMatch` requires an exact modifier match (e.g. **F4** alone does not trigger on **Alt+F4**) (`HotkeyBinding`, `HotkeyManager`).

### Changed

- Reverted v0.7.59 loose hotkey modifier policy — keyboard and mouse feature bindings now require Ctrl/Alt/Shift to match the binding exactly at runtime.

## [0.7.79] - 2026-07-04

### Added

- Configurable **보정 영역 (템플릿 대비)** % for ROI correction on loop 2+ (default 110%, range 50–300%, 5% step): per ImageFind block when per-block ROI correction is used, or in **기능 편집** when **전체 ROI 보정** is on (`roiCorrectionExpandPercent` JSON on feature and block, `ImageFindEditor`, `FeatureEditDialog`, `ExecutionContext`).

## [0.7.78] - 2026-07-04

### Changed

- ImageFind ROI units unified to **target-window percent** (0–100 of DWM bounds): user ROIs, ROI correction session cache, and `ScreenCapture::captureRegionFromPercent` all resolve via `resolveWindowPercentRegion`; legacy absolute-pixel `customRegions`, pixel-offset anchored JSON, and `ScreenPercent` virtual-desktop % auto-migrate on load (`ImageFindBlock`, `ExecutionContext`, `ScreenCapture`, `ImageFindEditor`, `MainWindow`).

## [0.7.77] - 2026-07-04

### Changed

- Agent policy: **every version bump at task close** must include GitHub **backup** (`git push origin main`) and **release** (`scripts/create-github-release.ps1`); documented in AGENTS.md §3.6, §9, §10 and `.cursor/rules/changelog-versioning.mdc`, `build-policy.mdc`, `ai-governance.mdc`, `immediate-handover.mdc`.

## [0.7.76] - 2026-07-04

### Changed

- Anchored ImageFind ROI storage switched from window pixel offset to **window-relative percent** (0–100 of target DWM bounds) so ROI follows both window move and resize (`ScreenCapture::resolveWindowPercentRegion`, `ImageFindBlock::customRegionsWindowPercent`, `ImageFindEditor`); legacy anchored pixel-offset JSON auto-migrates on load.

## [0.7.75] - 2026-07-03

### Changed

- Feature **대상 창 중앙 마우스 고정** and **마우스 위치 잠금** now anchor to the target window and refresh while the window moves (`MouseCenterLock`, `MainWindow`, `FeatureRunSession`).
- ImageFind **탐색 ROI** picked or edited with a configured target window is stored relative to that window (`customRegionsAnchoredToTargetWindow` JSON, `ScreenCapture`, `ImageFindBlock`, `ImageFindEditor`); workflow capture, ROI flash overlay, and match test resolve to physical pixels each poll.
- Mouse block editor labels/tooltips: **화면 중앙** → **대상 창 중앙** (`ClickEditor`).

## [0.7.74] - 2026-07-03

### Changed

- Interactive update check **현재 최신 버전입니다** feedback is now a 3 s title-bar transient status instead of a modal OK dialog (`UpdateChecker`, `MainWindow::onUpdateCheckFinished`).

## [0.7.73] - 2026-07-03

### Changed

- Main window bottom **업데이트** button stays enabled on **vX - 최신 버전입니다**; click re-runs an interactive GitHub update check (`MainWindow::onUpdateButtonClicked`, `refreshUpdateButtonState`).

## [0.7.72] - 2026-07-03

### Added

- Program settings **주기적으로 업데이트 확인** toggle and **확인 간격** (분) control: background GitHub update-check frequency is now configurable (1–1440 minutes) or fully disabled; persisted as `program/updateCheckIntervalMinutes` and applied live when settings change (`ProgramSettings`, `ProgramSettingsDialog`, `MainWindow::applyUpdateCheckInterval`).

## [0.7.71] - 2026-07-03

### Fixed

- In-app update restart: close the download progress dialog before shutdown, defer quit until the event loop is idle, hide the tray icon and main window, then `QApplication::exit(0)` so `PIPBONGUpdater` can replace files and relaunch (`UpdateChecker`, `MainWindow`).
- `PIPBONGUpdater`: wait until the main process actually exits (retry `OpenProcess` instead of assuming exit on failure), add a short post-exit delay before copying files, skip redundant `runas` when already elevated, and retry launch a few more times (`src/updater/main.cpp`).

## [0.7.70] - 2026-07-03

### Added

- Mouse block feature-run option **기능이 켜져 있는 동안 마우스 위치 잠금**: clips the physical cursor to the feature-start cursor position during the run session, with trigger preemption release/resume support and JSON `lockMouseToCurrentPositionDuringRun` (`MouseCenterLock`, `Feature`, `ClickEditor`, `MainWindow`, `JsonSerializer`).

## [0.7.69] - 2026-07-03

### Fixed

- Feature stop, pause, loop end, and shutdown now release PIPBONG-held mouse buttons (Click/KeyPress **누름**) in addition to tracked keyboard keys (`InputSimulator`, `ExecutionContext`, `MainWindow`, `WorkflowEngine`).

## [0.7.68] - 2026-07-03

### Fixed

- Feature list feature name text unreadable on some Windows dark-theme machines: derive name color from row `Base` luminance instead of stale `QPalette::Text` (`UiThemeColors::primaryContentTextColor`, `FeatureListPanel`).

## [0.7.67] - 2026-07-03

### Added

- ImageFind **이전 복귀** activation: purple glass pulse on both the failing block and the previous template-matching block jumped to (`BlockListWidget`, `WorkflowRunner`, `WorkflowEngine`, `MainWindow`).

## [0.7.66] - 2026-07-03

### Added

- Program settings **새 버전 감지 시 자동 업데이트** (`program/autoInstallUpdates`): silent GitHub update checks automatically start download/install when a newer release is detected; if workflow sessions are running, installation is deferred until all sessions stop (`ProgramSettings`, `ProgramSettingsDialog`, `MainWindow`).

## [0.7.65] - 2026-07-03

### Added

- Trigger mode preemption: when a trigger fires while other features are running, those sessions pause in place (workflow `ExecutionContext` pause), release **마우스 중앙 고정** temporarily, then resume from the same point after the trigger action; cursor is restored to the pre-trigger position so trigger mouse blocks do not affect the preempted run (`MainWindow`, `FeatureRunSession`).

## [0.7.64] - 2026-07-03

### Added

- Feature **트리거 모드** run mode: monitors only the first workflow ImageFind block with templates; on match runs the full workflow once, then waits `triggerCooldownMs` before monitoring again; hotkey or feature run button toggles the armed session (`FeatureRunMode::Trigger`, `MainWindow`, `WorkflowRunner`, `ImageFindBlock`, `FeatureEditDialog`, JSON `triggerCooldownMs`).

## [0.7.63] - 2026-07-03

### Added

- ImageFind block **템플릿 색상** option in **매칭 설정**: **자동** / **흑백** / **컬러** controls whether grayscale haystack filtering runs during workflow and match test (`templateColorMode` JSON, `ImageMatcher`, `ImageFindBlock`, `ImageFindEditor`).

## [0.7.62] - 2026-07-03

### Added

- Feature option **기능이 켜져 있는 동안 마우스를 화면 중앙에 고정** in the mouse block editor; clips the physical cursor to the virtual-screen center for the feature run session via `MouseCenterLock` (`Feature`, `ClickEditor`, `MainWindow`, JSON `lockMouseToScreenCenterDuringRun`).

## [0.7.61] - 2026-07-03

### Fixed

- **창 지정** pick mode title-bar hint ("대상 창을 클릭하세요…") now clears when pick completes or is cancelled instead of reappearing after the transient status expires (`MainWindow`).

## [0.7.60] - 2026-07-03

### Fixed

- **창 목록** picker now shows a pulsing border on the selected window while browsing the list, and plays the selection wave animation on the chosen HWND after confirm (no longer depends on title-based window lookup during overlay ticks; `TargetWindowHighlightOverlay`, `MainWindow`).

## [0.7.59] - 2026-07-03

### Changed

- Feature global hotkeys: keyboard bindings now use a low-level hook (same path as **누를 동안** / hold) instead of relying only on `RegisterHotKey`/`WM_HOTKEY`; `RegisterHotKey` + host `WM_HOTKEY` handler remain as fallback when hook install fails (`HotkeyManager`, `MainWindow`).
- Keyboard/mouse hotkey modifier matching allows extra held modifiers (e.g. accidental Shift on laptops) when they are not part of the binding (`HotkeyBinding::modifiersMatch`).

### Fixed

- Hotkey host window handles `WM_HOTKEY` directly in its Win32 proc so Qt message-filter gaps on some systems no longer drop registered hotkeys (`HotkeyManager`).

## [0.7.58] - 2026-07-03

### Changed

- Title bar icon easter egg: exit animation bounces smaller and flies back into the title bar icon instead of fading out; max size increased to ~94% of screen short edge; anchor tracks the badge widget (`TitleBarIconEasterEgg`, `CustomTitleBar`).

## [0.7.57] - 2026-07-03

### Changed

- Custom title bar left app icon enlarged from 22×22 to 30×30 px to better fill the title bar height (`CustomTitleBar`).

## [0.7.56] - 2026-07-03

### Fixed

- Title bar **항상 위** checkbox no longer clips its bottom border: slightly taller title bar (42 px), balanced bottom margin, and explicit indicator sizing (`CustomTitleBar`).

## [0.7.55] - 2026-07-03

### Added

- Feature **기능 편집** option **마우스 위치 복귀**: when enabled, restores the cursor to its screen position at workflow start when the feature run session ends (`Feature`, `FeatureEditDialog`, `JsonSerializer`, `FeatureRunSession`, `MainWindow`).

## [0.7.54] - 2026-07-03

### Added

- ImageFind **매칭 테스트** overlay hit labels now show capture + matching duration in ms beside the confidence score (e.g. `0.92 · 48 ms`) (`ImageFindBlock::testMatchTemplates`, `MatchTestOverlay`, `ImageFindEditor`).

## [0.7.53] - 2026-07-03

### Added

- **창 지정** pick mode now cancels on right-click as well as Esc, and successful target selection plays a center-out wave fill animation across the target window (`WindowPicker`, `TargetWindowHighlightOverlay`, `MainWindow`).

## [0.7.52] - 2026-07-03

### Added

- **창 목록** picker now shows each visible window with its process icon and flashes the selected target window border after confirmation (`MainWindow`, `TargetWindowHighlightOverlay`).

## [0.7.51] - 2026-07-03

### Added

- Title bar app icon easter egg: clicking the left PIPBONG icon pops a giant bouncing app icon over the current screen, then fades it out (`CustomTitleBar`, `TitleBarIconEasterEgg`).

## [0.7.50] - 2026-07-03

### Added

- `README.md` GitHub landing page (Korean): logo, badges, feature highlights, install and quick-start, collapsible build section, links to `AGENTS.md`. AGENTS.md documents README as the only user-facing doc exception to the single-document policy.

## [0.7.49] - 2026-07-03

### Added

- **창 지정** pick mode: hovering a top-level window shows a pulsing sky-blue border on that window; overlay follows the cursor target and dismisses on click, Esc, or cancel (`WindowPickerHoverOverlay`, `WindowPicker`).

## [0.7.48] - 2026-07-03

### Fixed

- In-app update now reliably restarts PIPBONG after install: updater skips overwriting its own running executable, retries launch with admin elevation when **항상 관리자 권한으로 실행** is enabled, and the main app forces a full quit (not tray hide) before the updater runs (`PIPBONGUpdater`, `UpdateChecker`, `MainWindow`).

## [0.7.47] - 2026-07-03

### Changed

- Workflow panel title keeps the last loop duration and session average after a run stops; values persist when switching features and clear only when a new run starts (`MainWindow`, `WorkflowEditorPanel`).

## [0.7.46] - 2026-07-03

### Added

- Program settings: **항상 관리자 권한으로 실행** sets Windows `RUNASADMIN` compatibility for the current `PIPBONG.exe` path; optional immediate elevated relaunch after enabling (`WindowsRunAsAdmin`, `ProgramSettings`, `ProgramSettingsDialog`, `MainWindow`).

## [0.7.45] - 2026-07-03

### Added

- Program settings: **Windows 시작 시 PIPBONG 자동 실행** registers the app in the current-user Windows Run key (`ProgramSettings`, `WindowsLaunchAtStartup`, `ProgramSettingsDialog`).
- Program settings: **닫기 시 트레이로 최소화** keeps PIPBONG running in the notification area when the window is closed (X); tray menu **열기** / **종료**; bottom **종료** and **파일 → 종료** still fully quit (`MainWindow`, `ProgramSettings`).

## [0.7.44] - 2026-07-03

### Added

- Workflow block list: **이전 복귀** and **재시도** columns show per-run activation counts for ImageFind **매칭 실패 시 바로 이전 템플릿 매칭 블록으로 돌아감** and **바로 다음 동작 후 다시 감지 시도** options; counts persist when switching features during a run (`ExecutionContext`, `WorkflowRunner`, `BlockListWidget`, `WorkflowEditorPanel`, `MainWindow`).

## [0.7.43] - 2026-07-02

### Fixed

- Calculator **수식 만들기** no longer crashes on open: operand rows are created only after the preview widgets exist, and `rebuildFormulaPreview` guards against uninitialized controls (`FormulaBuilderDialog`).

## [0.7.42] - 2026-07-02

### Added

- Calculator spreadsheet: selecting a formula cell now highlights referenced input cells with a gentle pulsing overlay so dependencies are visible at a glance (`CalculatorDialog`, `FormulaEvaluator`, `SpreadsheetCellDelegate`).

## [0.7.41] - 2026-07-02

### Changed

- Calculator: long inline hint text removed; **도움말** button opens a formatted help dialog with sectioned bullet lists (`CalculatorDialog`).

## [0.7.40] - 2026-07-02

### Added

- Main window bottom-left **업데이트** button: silent GitHub check on startup and every 5 minutes; shows **vX 버전으로 업데이트** when a newer release exists, otherwise disabled **vX - 최신 버전입니다** (`MainWindow`, `UpdateChecker`).

### Changed

- In-app update checker uses a persistent `UpdateChecker` with silent vs interactive modes; GitHub API requests send `Accept: application/vnd.github+json` (`UpdateChecker`, `MainWindow`).

## [0.7.39] - 2026-07-02

### Added

- Calculator spreadsheet undo/redo: **Ctrl+Z** / **Ctrl+Y** (and **Ctrl+Shift+Z**) restore sheet edits including cell values, borders, colors, row/column insert/delete, cell moves, currency binding, and formula-builder apply; up to 100 steps (`SpreadsheetModel`, `CalculatorDialog`, `FormulaBuilderDialog`).

## [0.7.38] - 2026-07-02

### Changed

- Calculator **시세 연동** picker: dedicated **즐겨찾기** section at the top (separate from category list) shows base-currency-relative rates beside item names; removed duplicate favorites category from the combo (`CurrencyPickerDialog`, `CalculatorDialog`).

## [0.7.37] - 2026-07-02

### Added

- Main window target panel: added **창 목록** button under **창 지정** to pick the target from currently visible top-level desktop windows (`MainWindow`).

### Changed

- Window-list picker dialog now shows each window as `[HWND] process.exe - window title`, supports refresh, preselects the current target window, and accepts double-click to apply quickly (`MainWindow`, `ScreenCapture::queryWindowInfo`).

## [0.7.36] - 2026-07-02

### Changed

- GitHub: unified source and releases into single public repo **`Baegovda/PIPBONG`**; in-app update and `create-github-release.ps1` now target that repo instead of **`PIPBONG-releases`** (`PipbongVersion.h.in`, `UpdateChecker`, scripts, AGENTS.md §3.6).

## [0.7.35] - 2026-06-29

### Added

- Calculator **행 삭제** / **열 삭제** (toolbar + context menu): remove selected row/column range; cell data, borders, colors, and formula references shift up/left; refs to deleted cells become `#REF!` (`SpreadsheetModel`, `FormulaEvaluator`, `CalculatorDialog`).

### Changed

- `create-github-release.ps1`: after publishing a new release on **`Baegovda/PIPBONG-releases`**, deletes all older releases so only the latest is visible on the releases page.

## [0.7.34] - 2026-06-29

### Changed

- Release packaging: staged **`dist/PIPBONG/`** folder with `README.txt`, `VERSION.txt`, **`PIPBONG 실행.bat`**, Qt plugin subfolders, and pruned runtime DLLs; ZIP root is **`PIPBONG/`** (`package-release.ps1`, `deploy-qt.ps1`).
- In-app update: `PIPBONGUpdater --install-zip` strips a single top-level folder from release ZIPs before copying into the install directory (`src/updater/main.cpp`).

## [0.7.33] - 2026-06-29

### Changed

- Distribution model: **dynamic DLL layout** only — removed static single-exe preset (`PIPBONG_STATIC_BUILD`, `build-static/`, `cmake --preset static`); `deploy-qt.ps1` also copies vcpkg runtime DLLs; `package-release.ps1` ships `dist/PIPBONG-win64.zip` (`CMakeLists.txt`, `CMakePresets.json`, `scripts/*`).
- In-app update: downloads `PIPBONG-win64.zip` and installs with `PIPBONGUpdater --install-zip` (`PipbongVersion.h.in`, `UpdateChecker`, `src/updater/main.cpp`, `create-github-release.ps1`).
- Build policy rule renamed `.cursor/rules/build-policy.mdc` (replaces `static-single-exe-build.mdc`); AGENTS.md §3 distribution docs updated.

### Removed

- Static single-exe build path and `dist/PIPBONG.exe`-only release workflow.

## [0.7.32] - 2026-06-29

### Removed

- Prior-install data migration from **SuckbongMachine** / **poez** app folders (`Application` — no `migratePriorProjectFiles` / `migratePriorSettings`).
- One-off dev scripts: `probe-poeninja-types.py`, `scrape-poeninja-html.py`, `scrape-poeninja-js.py`, `list-unmapped-ko.py`, `patch-unmapped-ko.py`.
- Local build artifacts: `build/`, `build-static/`, `build-clangd/`, `dist/` (regenerated on next build).

### Changed

- **AGENTS.md** §4/§5.7/§7: drop obsolete prior-install and removed block-type (`Loop`, `Comment`) JSON docs.
- GitHub source repository **`Baegovda/PIPBONG`** visibility set to **public**; **`v0.7.32`** published to **`Baegovda/PIPBONG-releases`** (`PIPBONG.exe`, `PIPBONGUpdater.exe`).

## [0.7.31] - 2026-06-29

### Added

- Calculator per-cell base currency override: **셀 기준 화폐** / **셀 기준 화폐 초기화** toolbar buttons and context menu; global **기준 화폐** combo unchanged as default; ApiRef values and formula references convert using each cell's effective base; persisted in sheet JSON v4 `cellBaseCurrencies` (`SpreadsheetModel`, `CalculatorDialog`, `CurrencyPickerDialog`).

## [0.7.30] - 2026-06-29

### Added

- Calculator **행 추가** / **열 추가** buttons (toolbar + cell context menu): insert a blank row or column at the selected position; cell data, borders, colors, and formula references (`A1` etc.) at or after the insertion point shift automatically (`SpreadsheetModel`, `FormulaEvaluator::shiftFormulaReferencesAtOrAfter`, `CalculatorDialog`).

## [0.7.29] - 2026-06-29

### Changed

- Calculator formula builder **적용 셀** **표에서 고르기** assigns the spreadsheet's currently selected cell immediately (no pick mode); opening **수식 만들기** also uses the active/selected cell as the default target (`CalculatorDialog`, `FormulaBuilderDialog`).

## [0.7.28] - 2026-06-29

### Added

- Calculator cell color palette: **배경색 ▾** / **글자색 ▾** swatch menus on the toolbar (custom color + clear per channel); context menu **배경색 설정…**, **글자색 설정…**, **색상 초기화**; colors persist in sheet JSON v3 `colors` array and move with cell drag (`SpreadsheetModel`, `SpreadsheetCellColors.h`, `CalculatorDialog`).

## [0.7.27] - 2026-06-29

### Changed

- Calculator formula builder: per-gap **+ − × ÷** operator buttons between value rows (replacing a single global operator); each value row has a **( )** toggle to wrap that term in parentheses; formula preview rebuilds from rows + segments (`FormulaBuilderDialog`).

## [0.7.26] - 2026-06-29

### Changed

- Calculator formula builder **수식 미리보기** field is directly editable; live result preview updates while typing; value rows refresh the preview only when the preview field is not focused (`FormulaBuilderDialog`).

## [0.7.25] - 2026-06-29

### Changed

- Calculator formula builder: dynamic **값** rows with **값 추가** / **삭제** (minimum one row); selected operator joins all non-empty terms (e.g. `A1+B1+C1`); each row has its own **표에서 고르기** (`FormulaBuilderDialog`).

## [0.7.24] - 2026-06-29

### Added

- Calculator **drag-move cells**: drag a selected range to a new location (move, not copy); cell content and borders move together; sheet formulas update references that pointed at moved cells (`FormulaEvaluator::shiftFormulaReferencesInRegion`, `SpreadsheetModel::moveCellRange`, `CalculatorDialog`).

## [0.7.23] - 2026-06-29

### Fixed

- Calculator auto-refresh no longer clears the grid selection: economy snapshot and display-only recalculations use `refreshCellStates()` + `dataChanged` instead of `beginResetModel` / `endResetModel` (`SpreadsheetModel`).

## [0.7.22] - 2026-06-29

### Added

- Calculator **cell borders** (Excel-style): select cells then **테두리 적용 ▾** for all/outside/inside/top/bottom/left/right/none; per-edge border mask on cells, drawn in `SpreadsheetCellDelegate`; persisted in sheet JSON v2 `borders` array (`SpreadsheetBorders`, `SpreadsheetModel`, `CalculatorDialog`).

## [0.7.21] - 2026-06-29

### Added

- Calculator Excel-style **formula bar** above the grid: name box shows active cell or range (`A1`, `A1:C3`); **fx** field shows/edits cell content or `=formula`; Enter or focus loss commits to the selected cell (`CalculatorDialog`).

## [0.7.20] - 2026-06-29

### Added

- Calculator spreadsheet **text cells**: non-numeric input (without a leading `=`) is stored and displayed as plain text; persisted as JSON `kind: "text"`; text cells are not numeric in formulas (`SpreadsheetModel`).

## [0.7.19] - 2026-06-29

### Changed

- Calculator spreadsheet supports multi-cell selection (drag, Ctrl+click, Shift+click); Delete/Backspace and context-menu **선택 셀 지우기** clear all selected cells; formula-pick mode no longer resets to single selection (`CalculatorDialog`).

## [0.7.18] - 2026-06-29

### Changed

- Calculator number display: values with no fractional part show as integers (e.g. `0`, `5`) instead of fixed trailing zeros (`0.0000`); fractional values still respect **소수 자릿수** up to the configured maximum (`SpreadsheetModel::formatNumber`).

## [0.7.17] - 2026-06-29

### Added

- Calculator formula builder **배열 수식 (범위)**: apply one formula pattern to a drag-selected rectangle with relative A1 reference shifting (Excel-style fill); **적용 방식** combo (**한 셀** / **배열 수식**), **적용 범위** pick, confirm dialog, and manual section (`FormulaBuilderDialog`, `FormulaEvaluator::shiftFormulaReferences`, `CalculatorDialog`).

## [0.7.16] - 2026-06-29

### Added

- Calculator spreadsheet: **Delete** / **Backspace** clears selected cell(s) when not editing (`CalculatorDialog`).

## [0.7.15] - 2026-06-29

### Changed

- Calculator spreadsheet cells use center alignment for all content; API-linked cells center icon + value as one block (`SpreadsheetModel`, `SpreadsheetCellDelegate`).

## [0.7.14] - 2026-06-29

### Added

- Calculator **자동 새로고침**: optional interval in minutes (1–120, default 5) with checkbox; `QTimer` calls `refreshExchangeRates` while the dialog is open; persisted in `QSettings` (`CalculatorDialog`).

## [0.7.13] - 2026-06-29

### Added

- Calculator **소수 자릿수** option (0–8, default 4): `DragAdjustSpinBox` in `CalculatorDialog`, persisted in `QSettings` `calculator/decimalPlaces`; applies to number, API-linked, and formula cell display (`SpreadsheetModel::formatNumber`).

## [0.7.12] - 2026-06-29

### Added

- Calculator **수식 만들기**: modeless `FormulaBuilderDialog` with collapsible arithmetic manual, `+ − × ÷` operators, operand fields, live result preview, **표에서 고르기** grid pick (click or drag multi-cell; Esc cancel), toolbar/context-menu entry (`CalculatorDialog`, `FormulaBuilderDialog`).

## [0.7.11] - 2026-06-29

### Fixed

- Calculator API-linked cells no longer paint overlapping text: delegate skips default label draw and right-aligns icon + value (`SpreadsheetCellDelegate`); numeric/formula cells use right alignment (`SpreadsheetModel`).

### Changed

- Default base currency is **Exalted Orb** (`currency:exalted`) instead of Divine Orb (`SpreadsheetModel`, `CalculatorDialog`).

### Added

- Base currency combo: shared favorites (★ button, right-click add/remove, favorites listed first) via `EconomyFavoritesStore` (`CalculatorDialog`).

## [0.7.10] - 2026-06-29

### Added

- Calculator **시세 연동** picker: **즐겨찾기** category, right-click **즐겨찾기 추가/제거**, starred labels, and `QSettings` persistence (`EconomyFavoritesStore`, `CurrencyPickerDialog`).

## [0.7.9] - 2026-06-29

### Added

- Calculator spreadsheet: right-click cell context menu with **시세 연동** and **셀 지우기** (`CalculatorDialog`).

## [0.7.8] - 2026-06-29

### Added

- Calculator **시세 연동** picker: **카테고리** combo filters the item list (전체 + 14 poe.ninja categories); search still works within the selected category (`CurrencyPickerDialog`).

## [0.7.7] - 2026-06-29

### Added

- PoE2 economy Korean names for all 14 poe.ninja calculator categories (~638 items): `EconomyNameKo` + generated `EconomyNameKoData.inc` from PoE2DB (`scripts/fetch-poe2db-ko-names.py`, `PoeNinjaClient::localizeEconomyRates`).

### Changed

- Calculator **시세 연동** picker and rate cells show PoE2DB Korean item names for fragments, runes, essences, uncut gems, expedition items, and other non-currency categories (not only 화폐).

## [0.7.6] - 2026-06-29

### Added

- Calculator fetches all 14 poe.ninja PoE2 currency-exchange categories (화폐, 파편, 심연의 뼈, 미가공 젬, 혈통 보조 젬, 에센스, 영혼 핵, 우상, 룬, 징조, 탐험, 액체 감정, 균열 촉매, 베리시움) in parallel; **시세 연동** picker groups items by category (`PoeNinjaEconomyCategories`, `PoeNinjaClient`, `CurrencyPickerDialog`).

### Changed

- Economy item ids use `category:item` composite keys (legacy `divine` loads as `currency:divine`); base-currency combo still lists 화폐 category only (`SpreadsheetModel`, `CalculatorDialog`).

## [0.7.5] - 2026-06-29

### Fixed

- Calculator currency icons failed to load: PoE2 exchange API image paths are on `web.poecdn.com`, not `poe.ninja` (`poeNinjaAssetUrl`); `CurrencyIconCache::setRates` re-fetches when the resolved URL changes.

## [0.7.4] - 2026-06-29

### Changed

- Calculator last-refresh label uses Korea Standard Time (`Asia/Seoul`) instead of UTC (`CalculatorDialog`).
- Calculator **기준 화폐** combo: any indexed currency can be the rate base (converted from poe.ninja Divine-relative `primaryValue`); choice persists in `QSettings` (`SpreadsheetModel`, `CalculatorDialog`).
- Currency icons always accompany currency UI: placeholder icon while loading in spreadsheet ApiRef cells, **화폐 연동** picker, and base-currency combo (`CurrencyIconCache`, `SpreadsheetCellDelegate`, `CurrencyPickerDialog`).

## [0.7.3] - 2026-06-29

### Changed

- Calculator currency display names use official Korean translations from PoE2DB (`poe2db.tw/kr/Currency`): `CurrencyNameKo` maps all 49 poe.ninja currency ids to localized names; applied on API parse and when refreshing bound spreadsheet cells (`PoeNinjaClient`, `SpreadsheetModel`).

## [0.7.2] - 2026-06-29

### Added

- Calculator currency icons from poe.ninja `items[].image` URLs: disk-cached `CurrencyIconCache`, icon column in spreadsheet cells and **화폐 연동** picker (`CurrencyIconCache`, `SpreadsheetCellDelegate`, `CurrencyPickerDialog`, `PoeNinjaClient`).

## [0.7.1] - 2026-06-29

### Fixed

- Calculator poe.ninja fetch failed on refresh: PoE2 exchange API uses string currency ids (e.g. `alch`), not integers — `PoeNinjaClient::parseExchangeOverview` now parses string ids; league combo omits leagues without a snapshot version (`PoeNinjaClient`, `CalculatorDialog`).

## [0.7.0] - 2026-06-29

### Added

- **시세 계산기:** bottom **계산기** button opens modeless `CalculatorDialog` with spreadsheet grid (`SpreadsheetModel`, `FormulaEvaluator`); poe.ninja PoE2 Currency Exchange via `PoeNinjaClient` (`index-state` + `exchange/.../overview`); league picker, refresh, **화폐 연동** cells, `=A1+B1` formulas; persists sheet in `QSettings` (`CalculatorDialog`, `MainWindow`).

## [0.6.3] - 2026-06-29

### Fixed

- ImageFind block editor: **매칭 실패 시 이전 블록 복귀** and **다음 동작 후 재시도** checkboxes can be enabled together; removed mutual-exclusion UI (`ImageFindEditor`).
- Combined failure handling: first failure runs next block then retries when retry is enabled; on second failure prefers **return to previous** ImageFind when that option is set, otherwise advances to the next ImageFind (`WorkflowRunner`).

## [0.6.2] - 2026-06-29

### Fixed

- Restored first-launch import from prior app installs **SuckbongMachine** and **poez**: auto-save `project.json`, `templates/`, and `QSettings` (window layout, hotkeys, program options) into **PIPBONG** when the new data folder is empty or still contains only the default **예시 기능** stub (`Application`).

## [0.6.1] - 2026-06-29

### Removed

- Legacy **Loop** / **Comment** / **If** block types and editors (`LoopBlock`, `CommentBlock`, `LoopEditor`, `CommentEditor`, `BranchWorkflowEditor`); unknown JSON block types are skipped on load (`BlockFactory`, `BlockEditorDialog`, `CMakeLists.txt`).
- App data migration from pre-PIPBONG folders (`Application::migrateLegacyAppData`).
- Unused `ProgramSettings::showWorkflowRunFeedback`; `FeatureRunMode::Toggle`; `UserInputInterruptMode::None`; QSettings/template JSON legacy hotkey migration (`TemplateCaptureHotkeySettings`, `ImageFindBlock`, `KeyPressBlock`).
- Orphan `SbmVersion.h.in`; unused CMake **debug** build preset.

### Changed

- JSON load accepts only `ImageFind`, `Click`, `KeyPress`, `Wait` blocks; ImageFind saves `templates` array only (no mirror `template` field).

## [0.6.0] - 2026-06-29

### Changed

- Application rebrand **SuckbongMachine** → **PIPBONG**: executable `PIPBONG.exe`, updater `PIPBONGUpdater.exe`, Qt org/app name, window title, project file filter, Win32 overlay/hotkey class names, `PipbongVersion.h`, CMake target `PIPBONG`, vcpkg manifest `pipbong`, IDE launch **Run PIPBONG (Release)** (`CMakeLists.txt`, `Application`, `MainWindow`, `UpdateChecker`, overlays, scripts, `.vscode/`, rules, `AGENTS.md`).
- GitHub: public source **`Baegovda/PIPBONG`**, public releases **`Baegovda/PIPBONG-releases`** (`create-github-release.ps1`, git remote).
- Auto-save migrates from legacy `%LOCALAPPDATA%/SuckbongMachine/SuckbongMachine/` when the PIPBONG data folder is empty (`Application::migrateLegacyAppData`).

## [0.5.99] - 2026-06-29

### Added

- ImageFind block **바로 다음 동작 후 다시 감지 시도, 감지되지 않으면 다음 템플릿 매칭 블록 절차로 넘어감** (`retryAfterNextActionOnFailure` JSON): on first detection failure (miss limit), run the next block once then retry this ImageFind; on second failure jump to the next ImageFind block (`ImageFindBlock`, `ImageFindEditor`, `WorkflowRunner`, `ExecutionContext` defer-retry state).

## [0.5.98] - 2026-06-29

### Added

- ImageFind block **매칭 실패 시 바로 이전 템플릿 매칭 블록으로 돌아감** (`returnToPreviousImageFindOnFailure` JSON): on detection failure (miss limit), `WorkflowRunner` jumps to the previous ImageFind block (`ImageFindBlock`, `ImageFindEditor`, `WorkflowRunner`).

## [0.5.97] - 2026-06-29

### Added

- Feature **기능 편집** option **첫 시작 시 첫 번째 템플릿의 ROI 수정한 뒤 바로 시작** (`editFirstTemplateRoiOnStart` JSON): before run, editable `RoiPreviewOverlay` on the first ImageFind block with templates and custom ROIs; confirm saves ROI and starts workflow; Esc cancels (`Feature`, `FeatureEditDialog`, `JsonSerializer`, `MainWindow`).

## [0.5.96] - 2026-06-29

### Added

- Mandatory IDE build workflow policy: AGENTS.md §3.1 and §8.8, always-applied `.cursor/rules/ide-build-workflow.mdc` (Ctrl+Shift+B / F5 via `build-release.ps1`, tracked `.vscode/`, `cmake.enabled: false`, recovery checklist).

## [0.5.95] - 2026-06-29

### Fixed

- Restore IDE build workflow: `.vscode/tasks.json` default **Build Release** runs `scripts/build-release.ps1`; F5 uses `launch.json` + incremental Release build; `cmake.enabled: false` stops CMake Tools configure/vcpkg conflicts and “task already running” errors; track `.vscode` config in git; ignore `build-clangd/`.

## [0.5.94] - 2026-06-29

### Changed

- New ImageFind blocks default **탐지 재시도** to the last value saved in `QSettings` (`ui/state/imageFindEditor/lastPollIntervalMs`); updated whenever an ImageFind editor is applied (`ImageFindPollIntervalPrefs`, `WorkflowEditorPanel`, `ImageFindEditor`).

## [0.5.93] - 2026-06-29

### Fixed

- Wait and ImageFind ms spin boxes snap to the 5 ms grid on `editingFinished` / apply only — not on every `valueChanged` keystroke, so typing `10` no longer collapses to `0` mid-edit (`WaitEditor`, `ImageFindEditor`).

## [0.5.92] - 2026-06-29

### Fixed

- Incremental builds no longer recompile every translation unit on a version bump: `PIPBONG_VERSION` and update macros moved from global `target_compile_definitions` to generated `PipbongVersion.h` included only by `Application.cpp` and `UpdateChecker.cpp` (`CMakeLists.txt`, `PipbongVersion.h.in`).

## [0.5.91] - 2026-06-29

### Changed

- Workflow block list header resize: removed custom filler column, viewport clamping, and bespoke save/restore wrappers; use standard Qt `QHeaderView::Interactive` + `Stretch` on **요약** with `UiStateManager::registerHeader` persistence (`BlockListWidget`, `WorkflowEditorPanel`, `MainWindow`).

## [0.5.90] - 2026-06-29

### Changed

- AI build policy: run incremental `cmake --build build --config Release` at task close when C++/headers/`CMakeLists.txt` changed; skip docs/rules-only; static `dist/` still user-request only (`AGENTS.md` §3/§9/§10, `.cursor/rules/static-single-exe-build.mdc`, `changelog-versioning.mdc`, `ai-governance.mdc`).

## [0.5.89] - 2026-06-29

### Changed

- ROI correction search region on loop 2+ expands the first-loop match rectangle by ~10% (centered) instead of using the exact template size (`ImageFindBlock::maybeRecordRoiCorrection`).

## [0.5.88] - 2026-06-29

### Fixed

- Workflow block list header stays pinned to the viewport right edge: data-column widths clamp to the visible width, trailing `Stretch` filler absorbs slack, horizontal scroll is disabled, and layout re-syncs on resize/restore (`BlockListWidget`).

## [0.5.87] - 2026-06-29

### Added

- Workflow panel title shows session average loop time beside the last loop duration (`루프 N: X ms · 평균 Y ms`); cumulative average resets when a new run session starts (`FeatureRunSession`, `WorkflowEditorPanel`, `MainWindow`).

## [0.5.86] - 2026-06-29

### Changed

- **직전 매칭** mouse clicks use `clickAtMatchScreen`: optional `SetCursorPos` when not already near the match point, no 16 ms settle or 12 ms tap hold, batched DOWN+UP at cursor; removed 15 ms pre-click delay (`InputSimulator`, `ClickBlock`).

## [0.5.85] - 2026-06-29

### Fixed

- Workflow block list header resize: all data columns (`#` through **매칭**) use `Interactive` drag resize; trailing empty `Stretch` filler column absorbs panel slack instead of stretching **요약** mid-table (`BlockListWidget`).

## [0.5.84] - 2026-06-29

### Changed

- **현재 위치** mouse tap sends batched DOWN+UP in one `SendInput` with no 12 ms hold; coordinate lookup for click-feedback runs only after the click and only when **실행 위치 표시** is enabled (`InputSimulator`, `ClickBlock`, `ExecutionContext`).

## [0.5.83] - 2026-06-29

### Fixed

- **현재 위치** mouse clicks no longer move/settle the cursor before tapping; skip 16 ms settle when the cursor is already at the target (`InputSimulator::clickAtCursor`, `settleCursorAtScreenPos`, `ClickBlock`) — typical block time drops from ~45 ms to ~12 ms (tap hold only).

## [0.5.82] - 2026-06-29

### Fixed

- Workflow block list header no longer stops short of the panel edge: **요약** column uses `QHeaderView::Stretch` to absorb remaining width while other columns stay drag-resizable (`BlockListWidget`).

## [0.5.81] - 2026-06-29

### Fixed

- Workflow ImageFind **기준/감지** column shows the live peak match score on every miss retry and on final block failure, even when below threshold (`WorkflowRunner`, `WorkflowEngine`, `MainWindow`, `WorkflowEditorPanel` run-feedback cache).

## [0.5.80] - 2026-06-29

### Added

- Workflow block list: all columns use `QHeaderView::Interactive` — drag header dividers to resize; layout saved under `workflowBlockList/columns` (`BlockListWidget`, `WorkflowEditorPanel`, `MainWindow`).

## [0.5.79] - 2026-06-29

### Removed

- Workflow block list column resize/persistence: removed `Interactive` modes, hidden stretch filler column, `saveState`/`restoreState`, legacy layout migration, and `ui/state/blockList/header` settings hooks (`BlockListWidget`, `WorkflowEditorPanel`, `MainWindow`).

### Changed

- Workflow block list columns use a fixed layout: metric columns `Fixed` width, **요약** `Stretch` fills remaining space; no user column drag-resize (`BlockListWidget`).

## [0.5.78] - 2026-06-29

### Fixed

- Workflow block list **ROI 보정** column: checkbox indicator is center-aligned via `CenterCheckBoxDelegate` (`BlockListWidget`).

## [0.5.77] - 2026-06-29

### Changed

- Workflow block list **ROI 보정** column moved to the right edge (between **기준/감지** and **매칭**); **ROI 보정** and **매칭** use `Fixed` width with trailing stretch filler so both stay pinned at the viewport right like left columns (`BlockListWidget`).

## [0.5.76] - 2026-06-29

### Added

- Per-block ROI correction when feature **전체 ROI 보정** is off: **ROI 보정** checkbox in ImageFind block editor and checkable **ROI 보정** workflow column (`ImageFindBlock`, `ImageFindEditor`, `BlockListWidget`, `WorkflowEditorPanel`).

### Changed

- Feature **전체 ROI 보정** applies ROI correction to all ImageFind blocks; per-block `roiCorrection` JSON and UI are used only when the feature flag is off (`Feature`, `ExecutionContext`, `MainWindow`, `FeatureEditDialog`).

## [0.5.75] - 2026-06-29

### Changed

- ImageFind **탐색 ROI** toolbar: **미리보기** (blue), **추가** (green), **삭제** (red) buttons use colored styles for clearer affordance (`ImageFindEditor`).
- ROI preview overlay: **확인** / **추가** / **삭제** toolbar segments use distinct green/blue/red fills with bolder labels; ROI borders, fill tint, and resize handles are brighter; ROI index badge moved to the **top-left** corner (`RoiPreviewOverlay`).

## [0.5.74] - 2026-06-29

### Fixed

- Workflow block list column resize: **요약** through **매칭** use the same `Interactive` mode as **#** / **미리보기** / **동작** (removed **요약** `Stretch` and **매칭** `Fixed`); hidden trailing stretch filler absorbs extra viewport width so **매칭** stays at the right edge (`BlockListWidget`).

## [0.5.73] - 2026-06-29

### Changed

- Workflow ImageFind run ROI hint: padded outward border (10 px) stays visible for the whole block (not a 0.48 s flash); border sits outside the real capture haystack so polls no longer hide the overlay; sky-tint outline (`WorkflowRoiFlashOverlay`, `ImageFindBlock`).

## [0.5.72] - 2026-06-29

### Fixed

- Workflow block list **매칭** column pinned to the viewport right edge (`QHeaderView::Fixed`, no trailing filler); extra width goes to **요약** (`Stretch`); metric columns stay independently resizable (`BlockListWidget`).

## [0.5.71] - 2026-06-29

### Fixed

- Workflow block list column resize: trailing hidden stretch filler column absorbs extra viewport width so **요약** no longer shrinks when resizing **동작 시간**–**매칭**; each data column (including **요약**) resizes only via its own divider; legacy 9-column saved layouts still restore (`BlockListWidget`, `WorkflowEditorPanel`).

## [0.5.70] - 2026-06-29

### Added

- Workflow run: when an ImageFind block starts, its search ROI flashes as a faint border on the target window (~480 ms fade); uses the effective search area (including session **ROI 보정**); hidden before capture polls (`WorkflowRoiFlashOverlay`, `ImageFindBlock`, `MainWindow` dismiss on run end).

## [0.5.69] - 2026-06-29

### Added

- Feature **ROI 보정** option in **기능 편집** (shown for **무한 반복** or **N회 반복** ≥2): loop 1 uses user-configured ImageFind search areas; on match, records the matched template bounds as a session-only custom ROI for that block; loop 2+ searches only that rect; JSON `roiCorrection` (default off, omitted when false); corrected coordinates are never saved to blocks (`Feature`, `FeatureEditDialog`, `JsonSerializer`, `ExecutionContext`, `ImageFindBlock`, `WorkflowRunner`, `MainWindow`).

## [0.5.68] - 2026-06-29

### Added

- **마우스 클릭 피드백 애니메이션** dedicated settings dialog (**프로그램 설정 → 설정…**): grouped timing / shape / size controls, checkerboard live preview with looped animation, **다시 재생** and **기본값** (`ClickPointerFeedbackSettingsDialog`, `ClickPointerFeedbackPreviewWidget`, `ClickPointerFeedbackRenderer`).

### Changed

- Program settings no longer embeds click-feedback controls inline; shows a one-line summary and opens the dedicated dialog (`ProgramSettingsDialog`).

## [0.5.67] - 2026-06-29

### Fixed

- Workflow block list column resize: removed `Stretch` on the middle **요약** column (Qt resized the wrong neighbor and felt inverted); all columns are `Interactive`, **요약** auto-fills remaining viewport width, persisted header state is validated for 9 columns and remapped to interactive modes on restore (`BlockListWidget`, `WorkflowEditorPanel`, `MainWindow`).

## [0.5.66] - 2026-06-29

### Changed

- Workflow block list **미리보기** column: single click opens the block editor; double-click on other columns still opens edit (`WorkflowEditorPanel`, `BlockListWidget::PreviewColumn`).

## [0.5.65] - 2026-06-29

### Fixed

- ROI preview overlay: `drawRoiNumberBadge` and `drawSizeChip` used `QRect(left, top, width, height)` but passed right/bottom coordinates as width/height, drawing giant chip/badge rectangles over the game window (`RoiPreviewOverlay`).

## [0.5.64] - 2026-06-29

### Changed

- Daily dev build policy: `PIPBONG_SKIP_WINDEPLOY=ON` on `default` preset skips `windeployqt` on every incremental link (~3 s no-op / ~5 s single-file builds); one-shot `deploy-qt` CMake target and `scripts/deploy-qt.ps1` for Qt DLL deployment (`CMakeLists.txt`, `CMakePresets.json`, `scripts/build-release.ps1`, AGENTS.md §3).

## [0.5.63] - 2026-06-29

### Removed

- Dead legacy UI: `TemplateCaptureDialog`, `CaptureCanvas` (replaced by `ScreenRegionOverlay` + `CaptureConfirmDialog`), `ImageFindPreviewDialog`, `RoiPreviewWidget` (replaced by `RoiPreviewOverlay`), and `FeatureRunModeDialog` (merged into `FeatureEditDialog`); dropped `ImageFindBlock::captureRoiPreview` / `ImageFindRoiPreviewData` and unused `OpenCvQtUtil::drawRoiOverlay` / `drawMatchPreview` (`CMakeLists.txt`, `ImageFindBlock`, `OpenCvQtUtil`).

## [0.5.62] - 2026-06-29

### Removed

- **만약 (If)** workflow block and all related code: `IfBlock`, `IfEditor`, `BlockListIfDisplay`, block-list If chrome/goto pick mode, `BlockType::If`, `workflowJumpIndex` goto jumps, and If add/edit UI (`Block.h`, `BlockFactory`, `BlockListWidget`, `BlockEditorDialog`, `WorkflowEditorPanel`, `UiStrings`, `CMakeLists.txt`). Legacy JSON `"type": "If"` is skipped on load (`BlockFactory::fromJson` returns null). Workflow nesting depth API renamed to `ExecutionContext::enterNestingScope` / `leaveNestingScope` (still used by `Loop` blocks and workflow loop regions).

## [0.5.61] - 2026-06-29

### Added

- Main window **대상 창** panel: **창 표시** button below **창 지정** flashes a pulsing sky-blue border on the target window for ~2.4 s (`TargetWindowHighlightOverlay`, `MainWindow`).

## [0.5.60] - 2026-06-29

### Added

- Feature list **Ctrl+C** / **Ctrl+V** copies and pastes the selected feature (workflow, loop regions, settings); paste inserts below selection as **{name} 복사** with a new id and cleared hotkey; context menu **복사** / **붙여넣기** (`Feature::duplicateAsNewInstance`, `Project::insertFeature`, `FeatureListWidget`, `FeatureListPanel`).

### Changed

- `Feature::clone` now copies workflow loop regions via `Workflow::assignFrom`.

## [0.5.59] - 2026-06-29

### Added

- Feature list **Delete** key removes the selected feature (same confirmation flow as the **삭제** button; disabled while workflow edit controls are off) (`FeatureListWidget`, `FeatureListPanel`).

## [0.5.58] - 2026-06-29

### Changed

- ROI preview overlay visual refresh: slate/emerald palette, softer dim scrim, thin accent borders, subtle fills, circular resize handles, glass size/number chips, bottom hint banner, and cleaner segmented toolbar (`RoiPreviewOverlay`).

## [0.5.57] - 2026-06-29

### Added

- ROI preview overlay shows **ROI 1**, **ROI 2**, … badges on each region (matching the editor list order); active ROI green chip, inactive ghost gray (`RoiPreviewOverlay`).

## [0.5.56] - 2026-06-29

### Added

- Program settings **마우스 클릭 피드백 애니메이션** group: display duration, animation speed, shape (dot+rings / ring only / crosshair / square), color, core size, max expansion, ring count, line thickness, and max opacity — persisted in `QSettings` (`PointerFeedbackSettings`, `ProgramSettingsDialog`, `WorkflowMatchFeedbackOverlay`).

## [0.5.55] - 2026-06-29

### Changed

- ROI preview editable overlay toolbar: segmented pill bar with per-action accent (confirm green, add blue, delete red), shadow, hover tint, and Segoe UI labels; anchored to the active ROI top-left above the box edge (`RoiPreviewOverlay`).

## [0.5.54] - 2026-06-29

### Fixed

- Workflow block list **기준/감지** threshold drag targeted the wrong row: hit-test now uses viewport `columnAt` / `rowAtViewportY` instead of `indexAt(mapTo(widget))`, which shifted Y by the header height (`BlockListWidget`).

## [0.5.53] - 2026-06-29

### Added

- Workflow block list **시도 횟수** column shows ImageFind template detection poll count during runs (live on each retry, final on block finish); cached per feature when switching tabs (`ImageFindBlock`, `ExecutionContext`, `WorkflowEngine`, `BlockListWidget`, `WorkflowEditorPanel`, `MainWindow`).

## [0.5.52] - 2026-06-29

### Added

- Workflow block list **기준/감지** column: horizontal drag on ImageFind rows adjusts match threshold (0.01 step; Shift ×10, Ctrl ×100); persists to block JSON with undo (`BlockListWidget`, `WorkflowEditorPanel`).

## [0.5.51] - 2026-06-29

### Changed

- Feature editor pointer-feedback checkbox label shortened to **실행 위치 표시** (`FeatureEditDialog`).

## [0.5.50] - 2026-06-29

### Changed

- Click/match pointer visual feedback moved from program settings to per-feature **실행 시 클릭·매칭 위치 시각 피드백** in **기능 편집**; JSON `pointerVisualFeedback` (default on, omitted when `true`); run session snapshots the flag (`Feature`, `FeatureEditDialog`, `JsonSerializer`, `FeatureRunSession`, `MainWindow`).

### Removed

- Program-wide `program/runPointerVisualFeedback` (`ProgramSettings`, `ProgramSettingsDialog`).

## [0.5.49] - 2026-06-29

### Fixed

- Post-ImageFind **직전 매칭** clicks less often missed: 15 ms settle after match before click; `InputSimulator` retries `SetCursorPos` until cursor reaches target, waits 16 ms after move, holds button down 12 ms before up, and uses 8 ms multi-click gap (`ClickBlock`, `InputSimulator`).

## [0.5.48] - 2026-06-29

### Added

- Program settings **기능 실행 시 마우스 클릭·매칭 위치 시각 피드백** toggles target-window pulse overlay for ImageFind matches and Click block positions (`ProgramSettings`, `ProgramSettingsDialog`, `MainWindow`, `ClickBlock`, `WorkflowMatchFeedbackOverlay`).

### Changed

- Workflow run pointer overlay: Click blocks emit blue pulses; ImageFind keeps green/red match pulses; all gated by one program setting (consolidates existing `WorkflowMatchFeedbackOverlay` behavior).

## [0.5.47] - 2026-06-29

### Changed

- Editable **ROI 미리보기** overlay: larger resize hit zones (18 px inside, 6 px outside edge); hover/drag cursors use Win32 `IDC_SIZE*` / `IDC_SIZEALL` per handle direction (`RoiPreviewOverlay`).

## [0.5.46] - 2026-06-29

### Added

- ImageFind editable **ROI 미리보기** overlay: **확인** / **추가** / **삭제** toolbar attached above the active ROI box; same actions as block editor OK and ROI toolbar (`RoiPreviewOverlay`, `ImageFindEditor`).

## [0.5.45] - 2026-06-29

### Changed

- ImageFind **ROI 미리보기** overlay supports drag move and corner/edge resize for custom ROIs when the block editor is open; inactive ROIs are ghosts; list selection stays in sync (`RoiPreviewOverlay`, `ImageFindEditor`).

### Removed

- ImageFind **탐색 ROI** toolbar **수정** button — ROI edits happen directly on the preview overlay (`ImageFindEditor`).

## [0.5.44] - 2026-06-29

### Changed

- **전체 딜레이 추가** dialog default ms restores the last confirmed value from `QSettings` `ui/state/workflowEditor/lastBulkInsertWaitMs` (`WorkflowEditorPanel`).

## [0.5.43] - 2026-06-26

### Changed

- In-app update and `create-github-release.ps1` publish to public **`Baegovda/PIPBONG-releases`** (exe only) so **파일 → 업데이트** works while source stays in private **`Baegovda/PIPBONG`**.

## [0.5.42] - 2026-06-26

### Changed

- GitHub repository rename `sbm` → `PIPBONG`: update URL, in-app update repo id (`Baegovda/PIPBONG`), release script, and git remote.

## [0.5.41] - 2026-06-24

### Added

- In-app update: **파일 → 업데이트** checks GitHub Releases (`Baegovda/PIPBONG`), downloads `PIPBONG.exe`, and replaces the running app via `PIPBONGUpdater.exe` (`UpdateChecker`, `MainWindow`).
- `PIPBONGUpdater` helper executable and `scripts/create-github-release.ps1` for publishing `dist/` assets to GitHub Releases.

## [0.5.40] - 2026-06-24

### Changed

- ImageFind block editor auto-shows **ROI 미리보기** on the target window when the editor opens and a search ROI is configured; overlay dismisses when the editor closes or the page is hidden (`ImageFindEditor`, `BlockEditorDialog`).

## [0.5.39] - 2026-06-24

### Added

- If block **이후 이동**: **목록에서 선택** opens workflow block-list pick mode (click or drag to a block, like loop-region selection); Esc cancels (`IfEditor`, `BlockListWidget`, `WorkflowEditorPanel`, `BlockEditorDialog`).

## [0.5.38] - 2026-06-24

### Changed

- Workflow block list **구간 반복** / **만약** header rows: **편집** / **삭제** buttons sit inline immediately after the chip label instead of the far-right columns (`BlockListWidget`).

## [0.5.37] - 2026-06-24

### Changed

- Workflow block list **구간 반복** and **만약** header rows show inline **편집** / **삭제** buttons (right columns); double-click still opens edit (`BlockListWidget`, `WorkflowEditorPanel`).

## [0.5.36] - 2026-06-24

### Added

- If block **이후 이동**: optional jump to a `#` column block in the parent workflow after a successful `then` or `else` branch (`thenGotoBlock` / `elseGotoBlock` JSON, `IfEditor`, `WorkflowRunner`).

## [0.5.35] - 2026-06-24

### Changed

- ImageFind workflow matching: one grayscale conversion per capture poll shared across all templates; skip full `findAllTemplates` enumeration when no prior match regions were consumed (peak match is sufficient); `captureSearchAreaForImageFind` reuses cached target `HWND` without `GetWindowTextW` each poll (`ImageFindBlock`, `ScreenCapture`).
- `isMostlyBlack` uses sparse pixel sampling instead of full-frame `cvtColor` + mean (`ScreenCapture`).

## [0.5.34] - 2026-06-24

### Changed

- Mouse click input uses `SetCursorPos` + batched button `SendInput` again (instant cursor teleport, accurate coords); **직전 매칭** clicks skip `targetWindow()` lookup (`InputSimulator`, `ClickBlock`).

## [0.5.33] - 2026-06-24

### Changed

- Screen capture speed: DXGI Desktop Duplication keeps a cached desktop frame, uses 5 ms `AcquireNextFrame` timeout (was 100 ms), reuses cache on `WAIT_TIMEOUT` for static-screen polls; GDI fallback uses `CreateDIBSection` direct buffer (`DxgiScreenCapture`, `ScreenCapture`).

## [0.5.32] - 2026-06-24

### Fixed

- Workflow editor run feedback (동작 시간, 매칭 시간, match score/thumbnail, loop title timing) persists when switching to another feature and back; per-feature cache in `WorkflowEditorPanel` instead of clearing on `setFeature`.

## [0.5.31] - 2026-06-24

### Fixed

- Workflow mouse click **동작 시간** inflated by per-click `GetWindowTextW` on the target window: `findTargetWindow` now trusts cached `HWND` until `IsWindow` fails; title changes still clear cache via `setTargetWindowTitle` (`ScreenCapture`).
- **직전 매칭** clicks reuse cached physical screen coords from template match instead of `ClientToScreen` every block (`ExecutionContext::lastMatchScreenPoint`, `ImageFindBlock`, `ClickBlock`).

### Changed

- Screen-coordinate clicks batch absolute move + button in one `SendInput` with `MOUSEEVENTF_VIRTUALDESK` (`InputSimulator`).

## [0.5.30] - 2026-06-24

### Fixed

- Workflow mouse clicks no longer jump the cursor toward the bottom-right: restored `SetCursorPos` for screen moves instead of `MOUSEEVENTF_ABSOLUTE` without `MOUSEEVENTF_VIRTUALDESK`, which mis-mapped virtual-screen coordinates (`InputSimulator`).

## [0.5.29] - 2026-06-24

### Changed

- Mouse/keyboard input latency: batched `SendInput` absolute move + button/key tap in one syscall; removed legacy 30 ms key-tap and 50 ms multi-click sleeps on `mouseButton` / `sendXButton`; wheel repeat gap 50 ms → 10 ms; workflow worker raises timer resolution with `timeBeginPeriod(1)` during runs (`InputSimulator`, `WorkflowEngine`).

## [0.5.28] - 2026-06-24

### Fixed

- Workflow **만약** block delete (Delete key): selecting the If header, branch headers, or main row now removes the If block; selecting only a branch inline row removes that branch block (`WorkflowEditorPanel::selectedBlockRows`, `removeSelectedBlocks`).

## [0.5.27] - 2026-06-24

### Changed

- Workflow **만약** blocks use loop-region-style list chrome: condition header row, green **맞으면** / amber **아니면** branch sections with inline branch block rows, tinted overlays, and double-click to edit the If block or a branch block (`BlockListWidget`, `WorkflowEditorPanel`, `ifConditionDisplayLabel`).

## [0.5.26] - 2026-06-24

### Changed

- Workflow loop region fill uses a lighter overlay tint (no duplicate row cell tint); drag-select preview and header chip tint reduced for better block-list text readability (`BlockListWidget`).

## [0.5.25] - 2026-06-24

### Changed

- Feature list run control moved from toolbar to a per-row ▶/■ button on the left of each feature; click runs or stops that feature; Hold mode rows show a disabled play button; context menu **실행** / **중지** uses the clicked row (`FeatureListPanel`, `MainWindow::onFeatureRunRequested`).

## [0.5.24] - 2026-06-24

### Added

- Feature list toolbar **실행** button runs or stops the selected feature (same as hotkey toggle); context menu adds **실행** / **중지** (`FeatureListPanel`, `MainWindow::onRunWorkflow`).

## [0.5.23] - 2026-06-24

### Changed

- Mouse **뒤로 가기** / **앞으로 가기** icons: both thumb paddles on the left side of the mouse; upper paddle = back with ← badge, lower = forward with → badge (`HotkeyBindingIcon::drawMouseSideNavigationIcon`).

## [0.5.22] - 2026-06-24

### Changed

- Workflow loop region header row: custom `LoopRegionHeaderRowDelegate` paints accent bar, ↻ badge chip, bold title, and muted exit-condition text; selection uses stronger highlight tint (`BlockListWidget`).

## [0.5.21] - 2026-06-24

### Fixed

- Workflow block list empty after loop-region header rows: `refresh()` now builds table rows via `setLoopRegions` before `setBlockInfo` and no longer rebuilds the table after populating cells (`WorkflowEditorPanel`).

## [0.5.20] - 2026-06-24

### Changed

- Workflow loop region label: dedicated header row inserted above each region’s start block (between rows) instead of an overlay chip on the first block row (`BlockListWidget` table-row mapping, `WorkflowEditorPanel` selection/double-click).
- Loop region delete from header double-click menu no longer shows a confirmation dialog (`WorkflowEditorPanel::onLoopRegionLabelDoubleClicked`).

## [0.5.19] - 2026-06-24

### Added

- One-click Release build: repo-root `빌드.bat` and Cursor default task **Ctrl+Shift+B** via `scripts/build-release.ps1` (auto-configure if `build/` missing, kill running exe before link).

## [0.5.18] - 2026-06-24

### Fixed

- Grayscale templates no longer match saturated color UI: auto-detect low-chroma templates and reject haystack hits whose match region exceeds a channel-spread threshold (`ImageMatcher`, `ImageFindBlock` workflow + match test).

## [0.5.17] - 2026-06-24

### Added

- Workflow loop regions show grouped chrome on the block list: left accent bar, top/bottom bracket lines, row tint, and **구간 반복 · {condition}** label chip; double-click label opens edit/delete menu (`BlockListWidget`, `WorkflowEditorPanel`).

## [0.5.16] - 2026-06-24

### Changed

- Wait block UI terminology **지연** → **딜레이**: block type label, editor title, bulk **전체 딜레이 추가/삭제**, and run log messages (`UiStrings`, `WaitEditor`, `WorkflowEditorPanel`, `WaitBlock`).

## [0.5.15] - 2026-06-24

### Changed

- Workflow **구간 반복**: click **구간 반복** to enter drag-pick mode on the block list (highlight preview, `↻` on range start); release opens exit-condition dialog with locked range; Esc or button toggles off; right-click button opens **구간 반복 목록** for edit/delete (`BlockListWidget`, `WorkflowEditorPanel`, `WorkflowLoopRegionEditDialog`).

## [0.5.14] - 2026-06-24

### Changed

- Mouse click input path optimized: `SetCursorPos` plus button-only `SendInput` (replaces 3-event absolute move batch); skip modifier `GetAsyncKeyState` snapshot when the block has no Ctrl/Alt/Shift; multi-click gap 10 ms → 1 ms (`InputSimulator::clickAt`, `clickAtClient`).

## [0.5.13] - 2026-06-24

### Changed

- AI build policy: **do not** run `cmake --build` at task close by default; build **only when the user explicitly requests** (`AGENTS.md` §3/§9/§10, `.cursor/rules/static-single-exe-build.mdc`, `changelog-versioning.mdc`, `ai-governance.mdc`).

## [0.5.12] - 2026-06-24

### Changed

- **항상 위** checkbox moved to custom title bar beside minimize; **설정** button added left of bottom **종료** (opens program settings); title bar **설정** menu removed (`CustomTitleBar`, `MainWindow`).

## [0.5.11] - 2026-06-24

### Changed

- Main window bottom area: **로그** and **대상 창** panels side by side in a horizontal splitter (resizable; state persisted as `main/bottomHorizontal`) (`MainWindow`).

## [0.5.10] - 2026-06-24

### Changed

- Wait block delay fields use 5 ms steps (`ms`, `minMs`, `maxMs`); legacy JSON values snap on load; editor drag/wheel/typing and **전체 지연 추가** dialog use `snapWaitDelayMs` (`WaitBlock`, `WaitEditor`, `WorkflowEditorPanel`).

## [0.5.9] - 2026-06-24

### Changed

- ImageFind **탐색 ROI** toolbar buttons renamed **미리보기** / **추가** / **수정** / **삭제** (removed **ROI** prefix); order is preview → add → edit → delete (`ImageFindEditor`).

## [0.5.8] - 2026-06-24

### Added

- Shared theme-aware secondary text colors and `HintLabel` widget (`UiThemeColors.h`, `HintLabel`); dark-mode hints use readable sky-blue tint instead of `palette(mid)` / `#666`.

### Changed

- ImageFind ROI/template hints, Click editor hints, program settings, loop-region dialog, and block type-change note use `HintLabel`.
- Hotkey binding chip idle/capture styles are theme-aware on dark backgrounds (`FeatureEditDialog`, `HotkeyCaptureDialog`, `ClickEditor`, `ImageFindEditor`).
- Feature list muted column text and target-window detail captions use `secondaryHintTextColor` (`FeatureListPanel`, `TargetWindowDetailPanel`).
- `AnimatedTwoWaySwitch` inactive label uses readable secondary hint color.

## [0.5.7] - 2026-06-24

### Changed

- ImageFind **ROI 미리보기** button keeps label **ROI 미리보기** when overlay is on (checked state only; no **끄기** suffix) (`ImageFindEditor::updateRoiPreviewButton`).

## [0.5.6] - 2026-06-24

### Changed

- ImageFind **탐지 재시도** interval uses 5 ms steps (min 5 ms); legacy JSON values snap on load; drag/wheel/typing snap via `snapImageFindPollIntervalMs` (`ImageFindBlock`, `ImageFindEditor`).
- `DragAdjustSpinBox` drag step now uses `singleStep()` with Shift ×10 and Ctrl ×100 (aligned with `DragAdjustDoubleSpinBox`).

## [0.5.5] - 2026-06-24

### Changed

- Drag-adjust numeric fields require 4 horizontal pixels per step (was 1 px); Shift/Ctrl multipliers unchanged (`DragAdjustSpinMouse.h`).

## [0.5.4] - 2026-06-24

### Changed

- Workflow editor bulk wait buttons renamed **광역 지연 추가** / **광역 지연 삭제** → **전체 지연 추가** / **전체 지연 삭제** (`WorkflowEditorPanel`).

## [0.5.3] - 2026-06-24

### Added

- **프로그램 설정** dialog (**설정** menu): option **기능 실행 시 해당 기능을 자동으로 선택** (default on) persisted in `QSettings` `program/autoSelectRunningFeature` (`ProgramSettings`, `ProgramSettingsDialog`, `MainWindow::selectRunningFeatureForDisplay`).

## [0.5.2] - 2026-06-24

### Added

- Workflow-level **구간 반복**: `#` block range loop regions on the feature workflow (not a separate block) with exit conditions; **구간 반복** dialog in workflow editor; list row `↻` marker and tint; JSON `workflow.loopRegions` with 1-based `startBlock`/`endBlock` (`Workflow`, `WorkflowLoopRegion`, `WorkflowRunner`, `WorkflowLoopRegionsDialog`, `BlockListWidget`, `JsonSerializer`).

### Changed

- Workflow **블록 추가** toolbar no longer includes the legacy **구간 반복** block type (existing `Loop` blocks in saved projects still load and run).

## [0.5.1] - 2026-06-24

### Changed

- AI build policy: prefer incremental `cmake --build build --config Release` at task close only when C++/UI changed; no mid-task builds; no `cmake --preset` on every close; skip build for docs/rules-only or when user requests skip (`AGENTS.md` §3/§9/§10, `.cursor/rules/static-single-exe-build.mdc`, `changelog-versioning.mdc`, `ai-governance.mdc`).

## [0.5.0] - 2026-06-24

### Added

- **구간 반복** workflow block (`Loop`): nested `body` workflow repeats until an exit condition is met, then execution continues to the next block; conditions `DetectionFailed` / `DetectionSucceeded` / `LastMatchSuccess` / `LastMatchFailed`; `detectionMissLimit` for fail-fast ImageFind inside the loop (`LoopBlock`, `LoopEditor`, `BlockFactory`, `WorkflowEditorPanel`, `BranchWorkflowEditor`).
- `ExecutionContext::clearDetectionFailedFlag()` for per-iteration loop exit checks.

## [0.4.29] - 2026-06-24

### Changed

- Feature **사용자 입력 시** default is **완전 정지** (`Stop`); **사용 안 함** removed from the editor combo; legacy JSON `"None"` loads as `Stop` (`Feature`, `FeatureEditDialog`, `UserInputInterruptMode`, `JsonSerializer`).

## [0.4.28] - 2026-06-24

### Added

- Run keyboard restore: PIPBONG tracks synthetic KEYDOWN/KEYUP during workflow runs and releases any still-held keys when each loop iteration or feature session ends; user-held modifiers at session start are never tracked and stay down (`ExecutionContext`, `InputSimulator`, `WorkflowEngine`, `MainWindow::finishRunSession`).

## [0.4.27] - 2026-06-24

### Added

- KeyPress block **수정키만** mode: run Ctrl/Alt/Shift actions without a main key; **지우기** clears key input; JSON `useMainKey: false` (`KeyPressBlock`, `KeyPressEditor`, `InputSimulator::sendKey`).

## [0.4.26] - 2026-06-24

### Added

- KeyPress block modifier keys (Ctrl/Alt/Shift) support per-modifier **사용 안 함** / **탭** / **누름** / **뗌** actions; JSON `ctrlAction` / `altAction` / `shiftAction` (`KeyPressModifierActions`, `KeyPressEditor`, `InputSimulator::sendKey`); legacy `ctrl`/`alt`/`shift` booleans load as `Down`.

## [0.4.25] - 2026-06-24

### Fixed

- Workflow ImageFind match-feedback pulse overlay no longer affects template matching: `hideBeforeCapture()` clears and hides the Win32 overlay synchronously before each poll capture (`WorkflowMatchFeedbackOverlay`, `ImageFindBlock::execute`).

## [0.4.24] - 2026-06-24

### Fixed

- Drag-adjust numeric fields no longer crash on single click: removed synthetic mouse events that re-entered the line-edit `eventFilter` recursively (stack overflow); click now focuses the line edit directly (`DragAdjustSpinBox`, `DragAdjustDoubleSpinBox`).

## [0.4.23] - 2026-06-24

### Changed

- Drag-adjust numeric fields: single click enters text input; horizontal drag works anywhere on the field (including the line-edit area) after `startDragDistance`; shared mouse logic in `DragAdjustSpinMouse.h` (`DragAdjustSpinBox`, `DragAdjustDoubleSpinBox`).

## [0.4.22] - 2026-06-24

### Fixed

- ImageFind **ROI 수정** pick overlay now ghosts all existing ROIs including the one being edited (was excluded, so single-ROI edits showed no guide) (`ImageFindEditor::startRoiPick`).

## [0.4.21] - 2026-06-24

### Added

- `DragAdjustDoubleSpinBox`: decimal drag-adjust input without stepper arrows; Shift ×10 and Ctrl ×100 step multipliers per pixel (`ui/widgets/DragAdjustDoubleSpinBox`).
- Program-wide numeric-input policy: [§8.7](#87-drag-adjust-numeric-input-mandatory-default), `.cursor/rules/drag-adjust-numeric-input.mdc`.

### Changed

- All block-editor and feature-dialog numeric fields now use drag-adjust spin boxes (no up/down arrows): ImageFind **임계값** (`DragAdjustDoubleSpinBox`), Click X/Y, Wait ms, feature repeat/exit counts (`DragAdjustSpinBox` in `ImageFindEditor`, `ClickEditor`, `FeatureEditDialog`, `FeatureRunModeDialog`).

## [0.4.20] - 2026-06-24

### Changed

- Click block editor coordinate pick button label **마우스로 좌표 지정** → **좌표 기록 (3초)** (`ClickEditor`).

### Added

- ImageFind block editor **ROI 수정** button: re-pick selected ROI in the list; other ROIs shown as ghosts during edit (`ImageFindEditor`).

## [0.4.19] - 2026-06-24

### Fixed

- Click coordinate pick cancel (Esc or **마우스로 좌표 지정 취소**): countdown tooltip now hides immediately; block editor no longer parks off-screen during pick (`CursorPositionPicker`, `ClickEditor`).

## [0.4.18] - 2026-06-24

### Changed

- Click block editor: **직전 매칭** + **매칭 중심 기준 상대 좌표** now shows the same **좌표** group as **고정 좌표** (X/Y, mouse pick, cursor hotkey, live cursor, client-coords toggle) (`ClickEditor`).

## [0.4.17] - 2026-06-24

### Changed

- ImageFind block editor layout reorganized: **매칭 설정** group, template list with side preview and toolbar row, compact ROI section with hint (`ImageFindEditor`).

## [0.4.16] - 2026-06-24

### Added

- ImageFind multi-ROI search: `customRegions` JSON array, ROI list in editor (**ROI 추가** / **삭제**), ordered try-all-ROIs execution, multi-rect pick ghost + preview + match-test overlay (`ImageFindBlock`, `ImageFindEditor`, `ScreenRegionOverlay`, `RoiPreviewOverlay`, `MatchTestOverlay`).

## [0.4.15] - 2026-06-24

### Changed

- Feature list toolbar button label **수정** → **편집** (`FeatureListPanel`).

## [0.4.14] - 2026-06-24

### Changed

- Feature edit dialog: **연속 실패 허용** spin row is shown only when **연속 감지 실패 시 종료** is checked (무한 반복 / 누를 동안 modes) (`FeatureEditDialog`).

## [0.4.13] - 2026-06-24

### Added

- Click block editor **좌표 입력 단축키** (default **F1**, rebindable via `QSettings` `hotkeys/clickCursorPosition`): while the mouse block editor is open, presses fill current cursor coordinates into fixed/offset fields (`ClickCursorHotkeySettings`, `ClickEditor`).

## [0.4.12] - 2026-06-24

### Changed

- ImageFind block editor **탐지 재시도** interval: 1 ms minimum and 1 ms step (was 50 ms minimum / 50 ms step); uses `DragAdjustSpinBox` (`ImageFindEditor`); legacy JSON values below 1 ms clamp to 1 on load (`ImageFindBlock`).

## [0.4.11] - 2026-06-24

### Added

- Feature option **사용자 입력 시** (`userInputInterrupt` JSON): **일시정지** toggles workflow pause/resume on physical keyboard or mouse-button input (excluding movement and injected/PIPBONG input); **완전 정지** stops the run (`UserInputInterruptMonitor`, `FeatureEditDialog`, `ExecutionContext`, `MainWindow`).

## [0.4.10] - 2026-06-24

### Added

- Workflow block list **매칭 시간** column beside **동작 시간**: cumulative screen capture + template-match work for ImageFind blocks, excluding poll-interval sleep (`BlockResult::imageFindMatchDurationMs`, `ImageFindBlock`, `BlockListWidget`, `WorkflowEditorPanel`).

## [0.4.9] - 2026-06-24

### Changed

- ImageFind **ROI 영역 지정** pick overlay ghosts the current custom ROI as a faint dotted gray region (`ScreenRegionOverlay::StartPickOptions::existingRoiPhysical`, `ImageFindEditor`).

## [0.4.8] - 2026-06-24

### Added

- Workflow ImageFind run feedback: semi-transparent pulse animation on the target window at each detection attempt (peak match location), green on success and red on miss (`WorkflowMatchFeedbackOverlay`, `ExecutionContext`, `ImageFindBlock`, `MainWindow`).

## [0.4.7] - 2026-06-24

### Changed

- Workflow block list **기준/감지** column shows detected match confidence to 2 decimal places instead of 3 (`BlockListWidget`).

## [0.4.6] - 2026-06-24

### Added

- ImageFind block supports multiple templates per block with `templates` JSON array and `templateMatchMode` (`Any` = 하나라도, `All` = 모두); editor list UI with add/remove, animated mode switch, and combined match test (`ImageFindBlock`, `ImageFindEditor`).

## [0.4.5] - 2026-06-24

### Changed

- ImageFind ROI button renamed **ROI 선택** → **ROI 영역 지정**; toggles to **ROI 영역 지정 취소** while pick overlay is active and a second click dismisses the pick (`ImageFindEditor`).

## [0.4.4] - 2026-06-22

### Changed

- Click block editor: **이동만** moved from action combo to checkbox beside **대상**; hides **동작 설정** group when checked (`ClickEditor`).
- Click block editor action group title **마우스** → **동작 설정**; live cursor label uses theme sky-blue accent via `QPalette` (`ClickEditor`).

## [0.4.3] - 2026-06-22

### Added

- Click block **이동만** action (`MoveOnly`): moves cursor to target without button press; JSON `action: "MoveOnly"`; client path uses `WM_MOUSEMOVE` (`ClickBlock`, `InputSimulator`, `ClickEditor`).

## [0.4.2] - 2026-06-22

### Changed

- Feature edit dialog: **반복 횟수** row hidden for **무한 반복** and **누를 동안**; shown only for **N회 반복** (`FeatureEditDialog`, `FeatureRunModeDialog`).

## [0.4.1] - 2026-06-22

### Added

- Click block editor shows live cursor position (`현재 커서: X …, Y …`) updated every 50 ms; respects **클라이언트 좌표** toggle and target-window lookup (`ClickEditor`).

## [0.4.0] - 2026-06-22

### Added

- **If (만약)** workflow block: nested `then` / `else` branch workflows, conditions `LastMatchSuccess` / `LastMatchFailed` / `DetectionFailed` with optional `negate` (`IfBlock`, `BlockFactory`, JSON `type: "If"`).
- `WorkflowRunner`: shared linear workflow execution loop with optional UI hooks; used by `WorkflowEngine` and `IfBlock` branch execution.
- `IfEditor` + `BranchWorkflowEditor`: condition UI and mini branch block lists in `BlockEditorDialog`; **만약** add button and diamond preview icon in `WorkflowEditorPanel`.

### Changed

- Workflow block list **동작** column shows **분기** for If blocks (`UiStrings::blockTypeWorkflowActionName`).

## [0.3.21] - 2026-06-22

### Changed

- Mouse **뒤로 가기** / **앞으로 가기** icons: browser-style chevron badge on the side-button paddle instead of a plain side tab (`HotkeyBindingIcon::drawMouseSideNavigationIcon`, `WorkflowEditorPanel` click preview).

## [0.3.20] - 2026-06-22

### Added

- `HotkeyBindingIcon`: drawn key-cap and mouse-button icons for feature hotkeys (`ui/HotkeyBindingIcon`).

### Changed

- Feature list **키** column shows hotkey icons instead of text labels; unbound rows still show `—` (`FeatureListPanel`, `FeatureListItemDelegate`).

## [0.3.19] - 2026-06-22

### Added

- `DragAdjustSpinBox`: spin box without stepper arrows; drag horizontally to adjust value (Shift ×10, Ctrl ×100 per pixel) (`ui/widgets/DragAdjustSpinBox`).

### Changed

- Wait block editor delay inputs use `DragAdjustSpinBox` with separate `ms` label beside the field instead of a spin-box suffix (`WaitEditor`).

## [0.3.18] - 2026-06-22

### Changed

- KeyPress block editor: **키보드** key field and **동작** combo on one row; **조합키** (Ctrl/Alt/Shift) on the row below (`KeyPressEditor`).

## [0.3.17] - 2026-06-22

### Added

- Infinite-repeat exit condition: optional **연속 감지 실패 시 종료** in feature editor (`Feature::infiniteExitAfterConsecutiveMisses`, `FeatureEditDialog`); when enabled for **무한 반복** or **누를 동안**, stops after N consecutive loop iterations where template matching fails (`MainWindow`, `ExecutionContext::imageFindMaxMissAttempts`, `ImageFindBlock`); persisted as JSON `infiniteExitAfterConsecutiveMisses`.

## [0.3.16] - 2026-06-22

### Added

- `AnimatedTwoWaySwitch`: pill track with sliding circular thumb and `QPropertyAnimation` between two labeled sides (`ui/widgets/AnimatedTwoWaySwitch`).

### Changed

- Wait block editor: **고정** / **랜덤** mode uses the animated two-way switch; input row below shows `[ms]` for fixed or `[ms]~[ms]` for random (`WaitEditor`).

## [0.3.15] - 2026-06-22

### Changed

- Block type UI labels: **클릭** → **마우스**, **이미지 찾기** → **템플릿 매칭**, **대기** → **지연** in block editor type buttons, editor window titles, workflow add buttons, and bulk-wait tools (`UiStrings`, `ClickEditor`, `ImageFindEditor`, `WaitEditor`, `WorkflowEditorPanel`, `ImageFindBlock::summary`); workflow **동작** column shows **템플릿** for ImageFind blocks.

## [0.3.14] - 2026-06-22

### Fixed

- Click/KeyPress block modifiers no longer stay down into the next workflow step: `releaseAppliedModifiers` compares against a pre-block `ModifierSnapshot` so PIPBONG-applied Ctrl/Alt/Shift are released after Tap while user-held keys are not (`InputSimulator`).

## [0.3.13] - 2026-06-22

### Changed

- KeyPress block editor: label **키** → **키보드**; block picker **키 입력** → **키보드**; Ctrl/Alt/Shift on one row (`KeyPressEditor`, `UiStrings.h`).

## [0.3.12] - 2026-06-22

### Changed

- Build policy: default preset is fast dynamic (`build/Release/`); static single-exe `dist/` build only when the user explicitly requests distribution (`CMakePresets.json`, `.cursor/rules/static-single-exe-build.mdc`, AGENTS.md §3 / §9 / §10).

## [0.3.11] - 2026-06-22

### Changed

- Click block editor: **뒤로 가기** / **앞으로 가기** use dedicated button categories with **동작** 클릭/누름/뗌; side-button X1/X2 down/up/tap batched with cursor move in `InputSimulator::clickAt` (`ClickEditor`, `ClickBlock::summary`).

## [0.3.10] - 2026-06-22

### Changed

- Click block editor: **버튼** `휠` unifies wheel click/scroll; **동작** shows 올림/내림/클릭/누름/뗌 for wheel and 클릭/누름/뗌 for other buttons (홀드/릴리즈 renamed) (`ClickEditor`, `ClickBlock::summary`).

## [0.3.9] - 2026-06-22

### Changed

- Distribution build policy: `default` CMake preset is static single-exe (`x64-windows-static`, `build-static/`); ship `dist/PIPBONG.exe` only. Optional `dynamic-dev` preset for local DLL builds (`CMakePresets.json`, `.cursor/rules/static-single-exe-build.mdc`, AGENTS.md §3 / §9 / §10).

## [0.3.8] - 2026-06-22

### Fixed

- Client-coordinate clicks no longer swallow input: `clickAtClient` converts to screen coords and uses atomic `SendInput` move+click via `clickAt` instead of a separate cursor move plus `SendMessage` (`InputSimulator`).

## [0.3.7] - 2026-06-22

### Fixed

- Client-coordinate clicks (including **직전 매칭**) now move the physical cursor to the target before `SendMessage` button events (`InputSimulator::clickAtClient`).

## [0.3.6] - 2026-06-22

### Added

- Static single-exe build preset: `cmake --preset static` with vcpkg `x64-windows-static`, `PIPBONG_STATIC_BUILD`, `qt_import_plugins`, and post-build copy to `dist/PIPBONG.exe` (`CMakePresets.json`, `CMakeLists.txt`).

## [0.3.5] - 2026-06-22

### Added

- Application icon from `Pipbong.ico`: Windows executable resource (`resources/PIPBONG.rc`), Qt embedded resource (`resources/app.qrc`), `QApplication::setWindowIcon`, and custom title bar badge pixmap (`Application`, `CustomTitleBar`).

## [0.3.4] - 2026-06-22

### Changed

- Feature list rows use a uniform background; removed alternating zebra striping and per-row running accent hues (`paintFeatureListRowChrome`).

## [0.3.3] - 2026-06-22

### Fixed

- Feature list **기능|방식** header divider drag moved opposite to the cursor: `modeColumnWidth` now decreases when dragging right (mode column right edge is anchored at the hotkey boundary) (`FeatureListHeaderWidget::applyDrag`).

## [0.3.2] - 2026-06-22

### Changed

- Feature list panel full visual redesign: grouped **기능 목록** layout with rounded table frame, styled toolbar buttons, custom row/header chrome (zebra rows, selection bar, running hue accent), and transparent list styling (`FeatureListPanel::setupUi`, `applyFeatureListPanelStyle`, `paintFeatureListRowChrome`, `paintFeatureListHeaderChrome`).
- Feature list **기능** / **방식** / **키** header labels and all row cell text center-aligned (`FeatureListHeaderWidget`, `FeatureListItemDelegate`, `drawCellText`, `paintFeatureName`).

## [0.3.1] - 2026-06-22

### Changed

- Feature list column resize simplified: fixed 4 px column gap; only two draggable header dividers (**기능|방식** → mode width, **방식|키** → hotkey width) plus row height; removed separate gap handles and overlapping hit zones (`FeatureListColumnLayout`, `FeatureListHeaderWidget`).

## [0.3.0] - 2026-06-22

### Added

- Feature list drag-and-drop reorder: row drag with blue insertion line; `Project::moveFeature`, `FeatureListWidget`, persisted feature order in project JSON (`FeatureListPanel`).
- Feature list panel column layout: drag header dividers to adjust **기능|방식** and **방식|키** gaps independently, **방식** / **키** column widths, and row height; values persist via `UiStateManager` (`ui/state/featureList/columns/*`, `FeatureListHeaderWidget`, `FeatureListColumnLayout`).
- Click block modifier keys: optional Ctrl/Alt/Shift during mouse actions (`ClickBlock::modifiers`, `ClickEditor`, `InputSimulator`); JSON `ctrl` / `alt` / `shift` on `Click` blocks.
- Concurrent feature runs: per-feature `WorkflowEngine` session (`FeatureRunSession`, `MainWindow::m_runSessions`); multiple features via hotkeys; per-feature repeat/hold loops; log lines prefixed with `[feature name]`.
- Custom frameless title bar (`CustomTitleBar`): **파일** menu, centered project title, SB badge, window controls, drag/double-click maximize, DWM rounded corners, edge resize hit-testing (`MainWindow`, `Qt::FramelessWindowHint`).
- Workflow panel title shows last completed loop timing (`루프 N: X ms (성공|실패)`) via `WorkflowEditorPanel::setLoopTiming`.
- Physical keyboard preservation policy: [§8.6](#86-physical-keyboard-state-during-workflow-runs-mandatory--do-not-regress), `.cursor/rules/physical-keyboard-preservation.mdc`.
- UX regression checklist for overlay/template capture: [§8.5](#85-template-capture-and-post-pick-ux-mandatory--manual-verify), `.cursor/rules/ux-regression-checklist.mdc`.

### Changed

- Versioning policy: **mandatory patch bump at end of every completed user task** — move `[Unreleased]` into a version section and update `CMakeLists.txt` before closing work ([§10](#10-versioning-policy), `.cursor/rules/changelog-versioning.mdc`, `.cursor/rules/ai-governance.mdc`).
- KeyPress block summary uses compact key names (e.g. `Ctrl+Space` tab) instead of raw VK codes (`HotkeyBinding::keyDisplayName`, `KeyPressBlock::summary`).
- Feature list column headers (**기능** / **방식** / **키**): theme-aware accent text, bold font, `AlternateBase` strip, divider line (`FeatureListHeaderWidget`).
- Custom title bar displays the same text as the Windows taskbar: status merged into `setWindowTitle()`; `CustomTitleBar` mirrors `windowTitle()` (`MainWindow::syncWindowTitleDisplay`, `CustomTitleBar::syncFromWindowTitle`).
- Feature list panel width no longer capped at 220 px; horizontal splitter can widen the panel.
- Custom title bar: removed **실행** menu (run/stop via feature hotkeys); **파일** menu layout margins adjusted.
- Feature list compact three-column layout with elided text and compact run-mode glyphs (`FeatureListPanel`, `FeatureListItemDelegate`).
- Workflow block list **동작** column: ImageFind shown as **이미지**; workflow editor tool row consolidated (`WorkflowEditorPanel`).
- Workflow block list run feedback: `QVariantAnimation` glass flash; removed continuous sin-wave glow (`BlockListWidget`).
- Log panel **로그** `QGroupBox` styling matches **대상 창** detail panel (`MainWindow::setupUi`).
- ImageFind match selection: top-leftmost threshold hit; peak used for fast miss detection (`ImageFindBlock::trySelectImageFindMatch`).
- Status messages (**자동 저장됨**, **실행 중**, etc.) in window title (taskbar + custom title bar) instead of bottom status bar (`MainWindow::showTransientStatus`, `setPersistentStatus`).

### Fixed

- Feature list header column resize: independent `nameModeGap` / `modeHotkeyGap`; one handle per property; closest-handle hit testing; dotted guide lines (`FeatureListColumnLayout`, `FeatureListHeaderWidget`).
- Custom title bar project title invisible: `QSizePolicy::Ignored` zero-width label fixed; explicit sync on title change (`CustomTitleBar`, `MainWindow`).
- Click/KeyPress modifier release no longer clears physically held Shift/Ctrl/Alt (`GetAsyncKeyState` guards, `InputSimulator`).
- Hold/click workflows no longer drop physically held Shift on loop end: no `AttachThreadInput`; client `SendMessage` clicks; hotkey swallow in LL hooks; removed `reassertPhysicallyHeldModifiers` (`HotkeyManager`, `InputSimulator`, `MainWindow`).
- Template capture UX: no off-screen editor park for capture path; overlay dismiss on editor close; cursor visibility after pick; BitBlt-only template PNG (`ImageFindEditor`, `ScreenRegionOverlay`, `ScreenCapture`, `BlockEditorDialog`).
- Application startup crash after custom title bar (`0xC00000FD`): no `setStyleSheet` from `changeEvent` (`CustomTitleBar`).

## [0.2.0] - 2026-06-22

### Added

- Main window bottom bar: **항상 위** checkbox left of **종료** toggles `Qt::WindowStaysOnTopHint`; preference persisted in `QSettings` (`ui/state/mainWindow/alwaysOnTop`, `MainWindow::applyAlwaysOnTop`).
### Changed

- Target window UI: removed title text field and **창 찾기** button; **창 지정** only, aligned to the right of the detail panel on one row (`MainWindow::setupUi`); target title stored via pick + project JSON only.
- Run launch warmup: `RunWarmup::prefetch` runs after app startup and **창 지정** (`MainWindow::scheduleRunWarmup`).
- Click block **직전 매칭** target: optional **매칭 중심 기준 상대 좌표** offset from the last match center (`lastMatchRelativeOffset`, `ClickEditor`, `ClickBlock::execute`).
- ImageFind block editor: optional **템플릿 캡처 단축키 (전역)** starts **화면에서 캡처** via a low-level keyboard hook only while the ImageFind editor page is visible in `BlockEditorDialog` (`TemplateCaptureHotkeySettings`, `ImageFindEditor`).
- Workflow block list **동작 시간** column (left of **기준/감지**): records each block's elapsed ms from start until completion before advancing to the next block (`WorkflowEngine::blockFinished`, `BlockListWidget`, `WorkflowEditorPanel`); cleared at each new run start.
- Workflow run log: each completed loop prints **루프 N: X ms (성공|실패)** on every iteration including repeat/hold sessions (`MainWindow::logLoopCompletion`, `QElapsedTimer` from `onEngineStarted` to `onEngineFinished`).
- Main window target-window detail line: HWND, title, class, DWM bounds, client size, process name, and visibility state after **창 지정** (`ScreenCapture::queryTargetWindowInfo`, `MainWindow::updateTargetWindowDetails`).
- Workflow block list clipboard: **Ctrl+C** / **Ctrl+V** copy and paste one or more selected blocks; paste inserts below the bottommost selected row (append when nothing selected); **Delete** removes selected blocks; extended row selection in `BlockListWidget`.
- Workflow undo/redo: **Ctrl+Z** / **Ctrl+Y** (or **Ctrl+Shift+Z**) per-feature workflow history in `WorkflowEditorPanel` (`Workflow::assignFrom`, up to 100 snapshots); history clears when switching features.
- Click block button types: **오른쪽**, **휠 클릭**, **휠 올림**, **휠 내림**, **뒤로 가기**, **앞으로 가기** (`MouseButton`, `InputSimulator`, `ClickEditor`); JSON `button`: `Back`, `Forward`, `WheelUp`, `WheelDown`; workflow **미리보기** icons for each button type (`WorkflowEditorPanel`).
- KeyPress block workflow **미리보기** icons: key-cap badge with compact key label (letters, digits, F-keys, arrows, specials) and modifier prefix (`C`/`A`/`S`) in `WorkflowEditorPanel`.
- Wait block workflow **미리보기** icon: drawn analog clock in `WorkflowEditorPanel::waitBlockPreviewIcon`.
- ImageFind **매칭 테스트** shows results on the target window via Win32 semi-transparent `MatchTestOverlay` (green hit boxes with confidence labels, click-through, Esc to close); `ImageFindBlock::testMatch` returns all threshold hits via `findAllTemplates`.
- ImageFind **매칭 테스트** toggle: second **매칭 테스트** click or **매칭 테스트 끄기** dismisses overlay without re-running; Esc uses low-level keyboard hook (`MatchTestOverlay`) when `RegisterHotKey` cannot receive focus.
- Workflow editor **광역 대기 삭제** button (was **대기 전부 삭제**) removes all Wait blocks from the current feature in one action with undo support (`WorkflowEditorPanel::onRemoveAllWaitBlocks`).
- Workflow editor **광역 대기 추가** button (was **사이에 대기 추가**) prompts for wait duration in ms and inserts that fixed delay between every adjacent pair of blocks (`WorkflowEditorPanel::onInsertWaitBetweenBlocks`, `QInputDialog`).

### Fixed

- Application startup crash (~4 s, exit `0xC00000FD` stack overflow): `TargetWindowDetailPanel::updateThemeColors()` reentrancy guard — `setStyleSheet` / child `setPalette` fired `StyleChange`/`PaletteChange` back into `changeEvent`, causing infinite recursion.
- Target window detail chip panel text unreadable in dark theme: replace stylesheet `palette()` text colors (often rendered black) with explicit `QPalette::WindowText` / muted caption colors and theme-aware state accents (`TargetWindowDetailPanel::updateThemeColors`).
- Target window detail panel background followed `QPalette::Base` / `AlternateBase` (white input-field colors in OS dark mode); panel/chip surfaces now use stylesheet `palette(window)` / `palette(button)` so they match the rest of the UI (`TargetWindowDetailPanel`).
- **항상 위** toggle no longer flashes the whole UI white: use Win32 `SetWindowPos` (`HWND_TOPMOST` / `HWND_NOTOPMOST`) instead of `setWindowFlags` + `show()` (`MainWindow::applyAlwaysOnTop`).
- First-run / hotkey launch stutter: `workflow.clone()` and `activateTargetWindow()` moved to the workflow worker via `WorkflowEngine::runPrepared`; hotkey first launch skips duplicate foreground activation (relies on `HotkeyManager::restoreForegroundWindow`); hotkey runs no longer call `selectFeatureById`/`refreshWorkflowEditor` during execution; repeat iterations reuse session clone with `ensureRunSessionResources(..., false)` (`MainWindow`, `WorkflowEngine`).
- ImageFind workflow success no longer leaves red miss visuals: match success is locked per row (miss updates ignored after success), completed blocks keep green ✓ styling when the next block starts, and success is committed on progress/finish (`BlockListWidget`, `WorkflowEditorPanel`, `MainWindow`).
- ImageFind workflow stuck on red miss despite **감지** peak **1.000**: consumed-region skip now applies only when multiple distinct on-screen instances exist (physical overlap merge), not when multi-scale duplicates inflate candidate count; single-target workflows accept the peak match without consumed rejection (`ImageFindBlock::trySelectImageFindMatch`, `countDistinctMatchInstances`).
- ImageFind workflow miss despite peak score above threshold: consumed match regions now use physical screen coordinates (consistent across TargetWindow vs CustomRegion haystacks), CustomRegion haystack capture uses `capturePhysicalRect`, saved ROI dimensions auto-enable `CustomRegion` on load/execute/match test, session workflow clone refreshes before every run iteration, and match test activates the target window like workflow runs (`ImageFindBlock`, `ScreenCapture`, `MainWindow`).
- Feature global hotkeys do not run workflows while the block editor dialog is open (`FeatureHotkeyGate`, `BlockEditorDialog`, `HotkeyManager`).
- Repeat/hold run restart stalls: reuse a persistent workflow worker thread (`WorkflowEngine`), cache cloned workflow/context for the run session (`MainWindow`), skip repeat-iteration log/UI churn, and cache ImageFind templates across iterations (`ImageFindBlock::cachedTemplateFor`).
- Workflow block execution highlight and match columns no longer apply to the currently selected feature when a different feature is running; block visuals update only when the displayed feature matches `m_runningFeatureId`, with state restored when switching back (`MainWindow`, `WorkflowEditorPanel::setFeature`).
- Adding a workflow block no longer registers the block when the editor is cancelled: `WorkflowEditorPanel::addBlockOfType` opens `BlockEditorDialog` first and adds to the workflow only on accept (was add-then-edit, leaving a default block on cancel).
- Feature run end stutter: `WorkflowEngine::stop()` no longer blocks the UI thread with `QThread::wait()`; shutdown paths use new `stopAndWait()` instead (`MainWindow::prepareForShutdown`, destructor). ImageFind poll sleep is interruptible in 50 ms steps so stop/hold-release responds within ~50 ms instead of up to `pollIntervalMs`; duplicate ImageFind miss highlight updates are skipped in `BlockListWidget::setActiveRow`.
- Workflow block list **기준/감지** baseline no longer stuck after ImageFind threshold edit: `WorkflowEditorPanel::refresh` reads `ImageFindBlock::threshold` from the model (with `setBlockMatchBaseline` when no run result yet) instead of cached `m_rowMatchThresholds` only.
- ImageFind retry feedback: each miss/retry emits `ImageFindMiss` and toggles the active block row flash (`BlockListWidget::notifyImageFindRetry`) instead of skipping visuals when highlight state is unchanged.
- ImageFind miss/retry row flash colors: much lower brightness and higher saturation crimson tones with light foreground text (`BlockListWidget::applyActiveRowVisuals`).
- ImageFind workflow detection mismatch vs **매칭 테스트**: use shared `ScreenCapture::captureSearchAreaForImageFind` (screen BitBlt first for TargetWindow — matches `capturePhysicalRect` templates; PrintWindow client buffer no longer used when BitBlt succeeds), activate target window on UI thread before workflow run (`MainWindow::launchWorkflowRun`), map TargetWindow match centers to client coords via DWM bounds (`ImageFindBlock::matchCenterToClickPoint`); preserve **창 지정** HWND when title unchanged, re-clone workflow each run start.
- ImageFind workflow **기준/감지** column updates on every miss retry with the peak template-match score from that attempt (`ImageMatcher::findPeakMatch`, `ExecutionContext::setLastMatchAttempt`, `WorkflowEngine` emits `blockMatchResult` on `ImageFindMiss` without thumbnail).
- ImageFind workflow match failure while peak score rounds to 1.00: `ImageMatcher::meetsThreshold` applies epsilon for OpenCV `TM_CCOEFF_NORMED` float peaks (strict `< threshold` rejected ~0.9999); workflow selection uses `trySelectImageFindMatch` with peak fallback and logs when hits are already consumed; **기준/감지** green/red follows actual match success (`blockMatchResult` `matched` flag, not threshold comparison alone); detected score shows 3 decimal places (`BlockListWidget`).
- Target window detail line not appearing after **창 지정** / **창 찾기**: moved controls into a dedicated **대상 창** group above the log (was nested under **실행**), show multi-line details with normal text color and minimum height, resolve HWND from **창 지정** even when the title field is empty (`ScreenCapture::queryWindowInfo`, `MainWindow::updateTargetWindowDetails`).
- ImageFind **ROI 선택** no longer parks the block editor off-screen (`ScreenRegionOverlay::StartPickOptions::parkHostDuringPick`); **화면에서 캡처** still parks the host before BitBlt.
- Match test overlay: confidence score labels render above each hit box in larger bold text (22 px Segoe UI) with a dark badge (`MatchTestOverlay::drawConfidenceLabel`).
- Workflow **매칭 결과** side panel removed entirely; ImageFind run feedback is shown only in the block list **매칭 점수** / **매칭** columns (`WorkflowEditorPanel`, simplified `WorkflowEngine::blockFinished` signal).

### Removed

- Block optional `label` field and editor **라벨** inputs for ImageFind and KeyPress; legacy JSON `label` keys are ignored on load and no longer written (`ImageFindBlock`, `KeyPressBlock`, `ImageFindEditor`, `KeyPressEditor`).
- ImageFind mouse-cursor ignore before template matching: removed `ScreenCapture::captureSearchAreaForMatching`, `eraseCursorFromCapture`, and cursor-region fill; `ImageFindBlock` execute and match test use `captureSearchArea` again.
- ImageFind **`ImageFindMatchTestDialog`** modal preview; replaced by in-game `MatchTestOverlay`.

### Changed

- Target window detail panel: replaced three-row metric chips with two compact inline lines (HWND/상태/프로세스/제목 · 클래스/DWM/클라/모니터); removed value elision that truncated monitor scale as `...` (`TargetWindowDetailPanel`).
- ImageFind performance: default `multiScale` is now **false** (1.0× only); workflow matching uses peak-first selection with `findAllTemplates` only when consumed regions require enumeration; miss feedback reuses the same peak (no duplicate `matchTemplate`); template scale grays and haystack grayscale are cached per poll (`ImageMatcher`, `ImageFindBlock`).
- Screen capture: DXGI Desktop Duplication is tried before GDI BitBlt for physical rect and target-window haystack capture, with BitBlt fallback (`DxgiScreenCapture`, `ScreenCapture`).
- ImageFind workflow match-success row feedback: fast glass glow pulse (~100 ms cycle, ~460 ms total) with specular vertical gradient and luminous left-edge accent on `#` column (`BlockListWidget`); removed infinite slow sine-wave fill.
- Click block execution no longer waits 30 ms after mouse move: `InputSimulator::clickAt` batches move + button down/up in one `SendInput` call (multi-click gap reduced from 50 ms to 10 ms).
- ImageFind **템플릿 캡처 단축키 (전역)** is stored in `QSettings` (`TemplateCaptureHotkeySettings`, key `hotkeys/templateCapture`); legacy `hotkeys/matchTest` and per-block JSON `matchTestHotkey` migrate once when unset; starts **화면에서 캡처** only while the ImageFind editor page is open in `BlockEditorDialog`.
- Target window detail line below **대상 창** controls redesigned as a three-row metric chip panel (`TargetWindowDetailPanel`: HWND/상태/프로세스, 제목/클래스, DWM/클라이언트/모니터) instead of a multi-line text block (`MainWindow::updateTargetWindowDetails`).
- Target window detail line adds **창 해상도(클라이언트)**, monitor number, monitor resolution, and effective DPI with scale percent for the display hosting the target window (`ScreenCapture::queryWindowInfo`, `MainWindow::updateTargetWindowDetails`).
- Feature list selection persists per project file in `QSettings` and restores on load/app start; list refresh tracks selection by feature id; hotkey runs no longer restore pre-run selection (avoids editor refresh during execution; running feature shown via rainbow highlight) (`FeatureListPanel`, `MainWindow::startFeatureRun`).
- Wait block editor: **랜덤 범위 사용** toggle shows a single **대기** ms field when off, or min~max ms range fields when on (`WaitEditor`, `BlockEditorDialog::fitToCurrentPage` on layout change).
- Feature run feedback: removed whole-panel dimming during execution (`FeatureListPanel::setEnabled`, `BlockListWidget::setEnabled`); running feature name shows a bold rainbow glow animation in the feature list (`FeatureListItemDelegate`); add/edit/delete buttons still disable while a workflow runs.
- Feature Hold run mode UI label **홀드를 누르는 동안** → **누를 동안** in `FeatureEditDialog`, `FeatureRunModeDialog`, `FeatureListPanel`, and run menu hint in `MainWindow`.
- Feature list toolbar button order: **추가**, **수정**, **삭제** in `FeatureListPanel`.
- Workflow block list column **유형** renamed to **동작**; **#**, **미리보기**, and **동작** column headers and cell text/icons center-aligned in `BlockListWidget` (preview icons via centered icon delegate).
- Removed hotkey capture hint labels from `FeatureEditDialog` and `HotkeyCaptureDialog`; binding label only.
- Mouse side-button hotkey labels: **측면 1/2** → **뒤로 가기** / **앞으로 가기** (`HotkeyBinding::displayString`, Win32 `VK_XBUTTON1` / `VK_XBUTTON2`).
- Workflow block list **미리보기** column shows drawn mouse icons for all Click block button types (`WorkflowEditorPanel`).
- Feature Hold run mode UI label renamed from **홀드** to **홀드를 누르는 동안** in `FeatureEditDialog`, `FeatureRunModeDialog`, and feature list run-mode column (`FeatureListPanel`); menu run hint updated in `MainWindow`.
- Merged **토글** run mode into **N회 반복**: 1회 equals former toggle (run once; same hotkey stops mid-run); removed **토글** from run-mode UI; JSON `"Toggle"` loads as `"RepeatCount"` and saves as `"RepeatCount"` (`FeatureRunMode`, `JsonSerializer`, `FeatureEditDialog`).
- Feature list toolbar simplified to **추가** / **삭제** / **수정**; double-click or **수정** opens **`FeatureEditDialog`** (name, hotkey, run mode) instead of separate rename/hotkey buttons and context-menu entries.
- Main window bottom panel order: **로그** above **대상 창** (swapped); removed idle **준비** label from target row; workflow/status hints use a hidden-until-needed status bar (`MainWindow::showTransientStatus`, `setPersistentStatus`).
- Main window bottom area (log + run controls) separated by a vertical splitter so log height can be adjusted and saved.
- ImageFind block editor: template preview shows pixel resolution (`해상도: W × H px`); template-capture hotkey binding and **단축키 지우기** on one row (3:1 stretch); form, template, and ROI sections center-aligned (`ImageFindEditor`).
- ImageFind block editor simplified: removed template path field, search-area combo, custom-region spin boxes, and screen-percent controls from the form; **탐색 ROI** group contains only **ROI 선택** and toggle **ROI 미리보기** buttons (Win32 semi-transparent overlay on the target window; inline thumbnail and ROI metadata label removed); template section uses **화면에서 캡처** only with **매칭 테스트**.
- Repositioned as a general-purpose window automation utility: main UI label **POE 창** → **대상 창**; default target title empty (placeholder **대상 창 제목**); overlay strings use **대상 창** instead of game-specific wording.
- Comment block removed from workflow add buttons and block-type picker; legacy `Comment` blocks in saved projects still load and edit.
- Application renamed from **poez** to **PIPBONG** (display name, executable `PIPBONG.exe`, Qt org/app name, `PIPBONG_VERSION`); legacy `%LOCALAPPDATA%/poez/poez` auto-save migrates on first launch when the new data folder is empty.
- Image matching pipeline rebuilt (`ImageMatcher`): `PreparedTemplate` with cached grayscale, `MatchOptions` / expanded `MatchResult` (center, matched size, scale), multi-scale template matching (default 0.9×/1.0×/1.1×), bounds validation, and consistent preprocessing; `ImageFindBlock` uses new API with optional JSON `multiScale` / `minScale` / `maxScale`; match-test UI shows scale and matched region size.
- ImageFind match selection: when multiple template hits exceed the confidence threshold in the search region, the top-leftmost match (smallest Y, then X) is chosen instead of the highest-confidence peak; overlapping duplicates are suppressed via non-maximum suppression; within a single workflow run, later ImageFind blocks skip already-selected regions and advance to the next top-left match (log shows `[n/total]` when multiple hits exist).
- Feature global hotkeys no longer steal focus to PIPBONG: hotkeys register on a hidden message-only Win32 host window and the previous foreground window is restored before the workflow runs; pressing the same feature hotkey again while that workflow is running stops it (`MainWindow::onHotkeyTriggered`).
- Block editor dialog: block-type selector changed from combo box to a horizontal row of toggle buttons at the top (**이미지 찾기**, **클릭**, **키 입력**, **대기**; legacy **주석** button only when editing a Comment block); `fitToCurrentPage()` sizes the stack to the **current** editor page only (fixes QStackedWidget using the largest hidden page width/height).
- Workflow block list row height reduced: template thumbnails **32×32** (was 48×48), default row height **36 px** with tighter cell padding in `BlockListWidget`.
- ImageFind block list summary no longer shows the template file path; uses label, screen-percent ROI text, or **이미지 찾기** fallback (`ImageFindBlock::summary`).
- Workflow block list: **기준/감지** column shows ImageFind threshold and detected confidence as `0.85 / 0.93` (매칭 기준 / 감지 점수) with a 32×32 cropped in-game match thumbnail in **매칭** when detection succeeds.
- Workflow block list drag reorder: animated blue insertion line with end markers shows the drop gap between rows while dragging; dragged row is dimmed until release (`BlockListWidget`).
- ImageFind execution: polls continuously until a match is found or the workflow is stopped; each miss reports **ImageFindMiss** progress to the UI; success reports **ImageFindSuccess**; workflow block list shows red **pulse** highlight (⌕) while searching, green highlight (✓) on match; removed unused `timeoutMs` / `failOnNotFound` fields; editor **재시도 간격** replaces **타임아웃**.
- Workflow editor: replaced **블록 추가** popup menu with per-type add buttons (**이미지 찾기**, **클릭**, **키 입력**, **대기**); edit/move controls grouped below; feature list panel narrowed; log panel height reduced; workflow area given more horizontal and vertical space.
- Main window run control bar: removed **실행** / **중지** buttons; run/stop remain on **실행** menu and feature hotkeys; **종료** moved to bottom-right below the target window controls.
- Main window title bar shows application version from `PIPBONG_VERSION` via `QCoreApplication::applicationVersion()` (e.g. `PIPBONG 0.1.0` or `PIPBONG 0.1.0 - project.json` when a file is open); removed **자동 저장** suffix.
- Project documentation consolidated into this single `AGENTS.md` file; removed separate `CHANGELOG.md`, `HANDOVER.md`, and `README.md`.

### Added

- **`FeatureEditDialog`**: unified feature editor (name, hotkey capture, run mode, repeat count); opened on list double-click or **수정** button; feature list toolbar reduced to **추가** / **삭제** / **수정** only; **홀드** run mode repeats the workflow while the bound key is held and stops on key release.
- Feature hotkeys support **mouse buttons** (left/right/middle/side) with optional Ctrl/Alt/Shift; click the hotkey label in **`FeatureEditDialog`** to arm capture, then the next key or mouse button is bound (`HotkeyCaptureDialog` uses the same label-click pattern); mouse-only bindings ignore unrelated keyboard modifiers at runtime and do not store accidental modifiers on capture; global mouse bindings use a low-level mouse hook (`HotkeyManager`) because `RegisterHotKey` is keyboard-only.
- **`UiStateManager`**: persists main-window geometry/maximize state, splitter positions (feature/workflow, main/log, block-list/tools), and block-list column widths via `QSettings` (`ui/state/*`); debounced auto-save on layout changes and flush on exit.
- Click block editor UX: **직전 매칭** hides the coordinate group and shows a short hint; **고정 좌표** uses a compact **좌표** group; **클릭** group puts **버튼** and **동작** on one row; count control removed from the editor; embedded layout uses content-sized width (no forced stretch); `BlockEditorDialog::fitToCurrentPage()` resizes the dialog to the active editor page.
- ImageFind ROI preview overlay: **ROI 미리보기** toggles `RoiPreviewOverlay` — semi-transparent Win32 overlay on the target window with green ROI highlight (click-through, Esc to close); `ScreenCapture::searchAreaPhysicalRect` resolves ROI bounds.
- Workflow run visual feedback: executing block row highlighted in amber with **▶** prefix and bold text; failed block shows red highlight with **✕**; active row scrolls into view; highlight restored after list refresh and cleared when the run finishes.
- Workflow block list run feedback: fast ~100 ms glass glow pulse for ImageFind miss/success/failed; success/failed highlights auto-clear (~460–640 ms); vertical specular gradients and luminous edge accents replace flat row fills (`BlockListWidget`).
- Workflow block list execution/drag feedback colors desaturated to softer pastels (running, miss pulse, success, failed, drop insertion line).
- Target window click-to-pick: **창 지정** button on the main window runs `WindowPicker` (crosshair cursor, global click selects top-level window, Esc cancels); fills **대상 창** title and caches `HWND` via `ScreenCapture::setTargetWindow` for capture and workflows.
- Click block **현재 위치** target (`ClickTarget::CurrentPosition`, JSON `target: "CurrentPosition"`): clicks at the mouse cursor position when the block runs; optional client-coordinate mode via `useClientCoordinates` (`ClickEditor`, `InputSimulator::getCursorScreenPosition`).
- Click block mouse action selector: **클릭** / **홀드** / **릴리즈** in `ClickEditor` (`ClickAction` in `InputSimulator`, JSON `action`: `Tap`/`Down`/`Up`); hold/release move to target then send button down/up only.
- ImageFind **화면 비율 (%)** search area: define detection ROI as X/Y/W/H percentages of the virtual desktop; JSON `searchArea: "ScreenPercent"` with `percentRegion`; ROI preview and match test supported.
- Agent docs for Win32 screen-region overlay: `.cursor/rules/screen-capture-overlay.mdc` (mandatory pattern — no Qt widget overlay regression).
- Workflow block list template thumbnails: **미리보기** column in `BlockListWidget` shows a 48×48 scaled preview for ImageFind blocks with a resolved template PNG; empty when no template or file is missing; refreshes on edit, capture, and project-directory change.
- Workflow block list drag-and-drop reordering: row drag in `BlockListWidget` with drop indicator and tooltip, `Workflow::moveBlock`, model sync via `workflowModified`, disabled while a workflow is running.
- ImageFind ROI and match-test preview (`RoiPreviewWidget`, `ImageFindPreviewDialog`, `ImageFindMatchTestDialog`): inline haystack preview with green ROI overlay, **ROI 미리보기** dialog with draggable custom region on full-screen capture, **매칭 테스트** dialog with success/failure indicator, confidence score, and match rectangle; `ImageFindBlock::captureRoiPreview` / `testMatch` helpers and `OpenCvQtUtil` for cv::Mat display.
- `CaptureConfirmDialog` after screen-region overlay pick: scaled template preview, selection metadata (dimensions, channels, estimated PNG size, average brightness), OK/Cancel before saving to `templates/`.
- Per-feature global hotkey binding to run workflows: `HotkeyManager` (`RegisterHotKey` / `WM_HOTKEY`), `HotkeyCaptureDialog`, feature list **단축키** button / double-click / context menu, hotkey display in list, JSON persistence under `hotkey`, duplicate-binding warning.
- Exit button in the run control bar and unified shutdown path (`MainWindow::onExitRequested` / `prepareForShutdown`: stop workflow worker, dismiss capture overlays, flush auto-save); confirm exit only when a workflow is running.
- AI governance docs: this file, `.cursor/rules/`, and changelog baseline.
- Single version source via `PIPBONG_VERSION` compile definition from CMake.

### Fixed

- Feature **홀드** run mode now repeats the workflow while the hotkey stays pressed and stops on release; fixed repeat restart blocked because `WorkflowEngine::finished` fired before `m_running` was cleared (`WorkflowEngine::onWorkerFinished`); fixed release stutter from a race where `onEngineFinished` restarted the workflow after the hook had already cleared hold state — hold continuation now defers via `QTimer::singleShot(0)` and reads synchronous hook `keyDown`/`buttonDown` via `HotkeyManager::isHoldBindingDown` instead of `GetAsyncKeyState`.
- ImageFind `CustomRegion` / `FullScreen` / `ScreenPercent` last-match click coordinates: map haystack-relative match centers to target-window client coordinates (or screen coordinates when no target window) so **직전 매칭** clicks land on the correct instance after ROI search.
- Screen region overlay Esc cancel: `ScreenRegionOverlay` installs a low-level keyboard hook during pick and posts cancel to the overlay HWND so Esc works when the target game steals focus; `RegisterHotKey` failure is handled; `dismissAll()` routes through `finishPick(false)` to restore the parked host.
- Workflow block drag-and-drop row vanishing: `BlockListWidget::dropEvent` now accepts with `Qt::IgnoreAction` so Qt `InternalMove` does not `clearOrRemove()` the source row; `blockRowsReordered` always fires after `startDrag` and `WorkflowEditorPanel` always `refresh()`es (model move only when indices differ).
- Screen capture overlay click-through and misplacement: replace Qt translucent widget with a Win32 topmost layered popup positioned in physical pixels over the POE DWM bounds; `RegisterHotKey` for reliable Esc cancel; native mouse capture for drag selection.
- Workflow block list drag reorder: defer `blockRowsReordered` until after `QTableWidget::startDrag` returns so Qt's post-drop `clearOrRemove()` no longer strips a row after `WorkflowEditorPanel::refresh()`; disable `Qt::ItemIsDropEnabled` on row items so drops target between rows.
- Screen capture overlay misplacement and dead mouse input: set per-monitor DPI awareness before `QApplication` init, position the overlay with native `SetWindowPos` in physical pixels over the POE window (instead of mismatched Qt `setGeometry` conversion), hide the host top-level editor dialog during pick, and map selection drawing via widget `devicePixelRatio()`.
- Screen capture overlay architectural rewrite: replaced nested `QEventLoop` / modal hide-restore hacks with async `ScreenRegionOverlay::startPick(host, callback)` — modeless top-level overlay over POE window bounds only, synchronous overlay teardown, host editor restored before confirm dialog; ESC and drag selection work without leaving invisible windows or blocking clicks.
- DPI mismatch between overlay selection and saved template capture: convert Win32 physical window bounds to Qt logical geometry for overlay placement, then map drag selection back to physical pixels via per-monitor `devicePixelRatio()` before `ScreenCapture::captureScreenRect` BitBlt.
- Residual screen capture position offset after Qt `devicePixelRatio()` conversion: centralize physical↔logical mapping in `ScreenCapture` via Win32 `PhysicalToLogicalPointForPerMonitorDPI` / `LogicalToPhysicalPointForPerMonitorDPI` (target window as DPI reference), track overlay selection in physical pixels with `GetCursorPos`, and capture through `capturePhysicalRect`.
- Screen capture template not saved after overlay pick: run `ScreenCapture::capturePhysicalRect` before restoring the hidden block editor host (BitBlt was capturing the editor window instead of the game), defer `CaptureConfirmDialog` and PNG save to the next event-loop tick after host restore, and call `ImageFindEditor::apply()` immediately after a successful capture save.
- Block editor closing after screen capture: `ScreenRegionOverlay::startPick` called `hide()` on `BlockEditorDialog` during `exec()`, which exits Qt's modal event loop with `Rejected`; park the host off-screen instead (`parkHostOffScreen` / `restoreHost`) so the editor stays open and template preview updates persist.

## [0.1.0] - 2026-06-21

### Added

- Initial C++/Qt6 desktop app for Path of Exile block-based automation.
- Feature list and workflow editor with block types: ImageFind, Click, KeyPress, Wait, Comment.
- Workflow engine with threaded execution, stop support, and runtime logging.
- Screen capture with multi-strategy fallback (PrintWindow, screen BitBlt, full-screen crop) for GPU-rendered games.
- Screen region overlay picker for in-game template capture (`ScreenRegionOverlay`).
- Template storage under project `templates/` directory.
- JSON project save/load via `JsonSerializer`.
- Korean UI localization (`QLocale::Korean`, `tr()` strings, Korean block log messages).
- Auto-save to `%LOCALAPPDATA%/poez/project.json` with 800 ms debounce and restore on startup.
- CMake preset and vcpkg dependencies (Qt6, OpenCV4, nlohmann-json).

### Fixed

- Black screen capture for borderless/windowed POE via DWM extended frame bounds and capture fallbacks.
- Overlay capture freeze by deferring confirm/capture out of mouse-release reentrancy.
- Immediate template confirm on mouse release (no Enter key required).

### Changed

- Default project directory moved from Documents to `QStandardPaths::AppDataLocation`.

---

## 12. Risk and Legal Notices

Using automation tools with Path of Exile may violate the game's **Terms of Service** and can result in account penalties. **Use at your own risk.**

This warning must remain in [§1 Project Overview](#1-project-overview) and user-facing materials. Do not remove it.

**Technical risk:** Administrator-elevated POE blocks input from non-elevated PIPBONG (see [overlay limitation](#known-limitation)).

---

## 13. Cursor Rules Summary

Always-applied rules live in `.cursor/rules/`. Essential content is inlined here and in sections above.

### `ai-governance.mdc`

- **100% AI-maintained** codebase.
- User replies: Korean. Code/docs/changelog: English. UI: Korean. JSON types: English.
- After every task: append `[Unreleased]` bullets, then **bump version before closing** ([§10](#10-versioning-policy)); minimal diffs.
- Primary documentation: **this file (`AGENTS.md`) only**.

### `changelog-versioning.mdc`

- Append `[Unreleased]` bullets in [AGENTS.md §11](#11-changelog-and-version-history) while implementing; **mandatory patch bump** before closing every completed user task ([§10](#10-versioning-policy)).
- Version source: `CMakeLists.txt` only.

### `screen-capture-overlay.mdc`

- Win32 overlay pattern is **mandatory** for in-game template capture.
- Full rules in [§8.1](#81-win32-screen-region-overlay-mandatory--do-not-regress).
- Do not regress to Qt widget overlay without explicit approval and regression testing.

### `ux-regression-checklist.mdc`

- Manual Windows checks for template capture, cursor visibility, dialog placement, and teardown order.
- Full checklist in [§8.5](#85-template-capture-and-post-pick-ux-mandatory--manual-verify).

### `qt-changeevent-stylesheet.mdc`

- Never call `setStyleSheet` from `changeEvent` on `StyleChange` (stack overflow `0xC00000FD`).
- Full rules in [§8.4](#84-qt-stylesheet-and-changeevent-mandatory--no-startup-crash).

### `physical-keyboard-preservation.mdc`

- User-held modifiers before a feature session must not be released on loop end; PIPBONG-applied keys are restored via tracked `m_pipbongHeldVirtualKeys`.
- Full rules in [§8.6](#86-physical-keyboard-state-during-workflow-runs-mandatory--do-not-regress): no `AttachThreadInput`, guarded modifier `SendInput`, session key tracking + `restoreRunKeyboard`, hotkey swallow in hooks, no blind keyboard sync.

### `drag-adjust-numeric-input.mdc`

- Default numeric inputs: `DragAdjustSpinBox` / `DragAdjustDoubleSpinBox` — no arrows; horizontal drag to adjust.
- Full rules in [§8.7](#87-drag-adjust-numeric-input-mandatory-default).

### `build-policy.mdc`

- **Task close:** incremental `cmake --build build --config Release` when C++/headers/`CMakeLists.txt` changed (changed `.cpp` only); skip docs/rules-only.
- **Version bump close:** mandatory `git push` + `create-github-release.ps1` ([§3.6](#36-github-backup-and-release)).
- Full policy in [§3](#3-build-and-run). **IDE/F5 path:** [§3.1](#31-ide--cursor-build-workflow-mandatory--do-not-regress) + `ide-build-workflow.mdc`.

### `ide-build-workflow.mdc`

- **Mandatory** Cursor/VS Code workflow: `scripts/build-release.ps1`, tracked `.vscode/tasks.json` + `launch.json`, **`cmake.enabled`: `false`**.
- Full rules and recovery in [§3.1](#31-ide--cursor-build-workflow-mandatory--do-not-regress) and [§8.8](#88-ide--cursor-build-workflow-mandatory--do-not-regress).
- Never re-enable CMake Tools configure-on-F5 or remove `.vscode` from git.

---

*Last consolidated: 2026-07-05. Current application version: 0.8.5.*
