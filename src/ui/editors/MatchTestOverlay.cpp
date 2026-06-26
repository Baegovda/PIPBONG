#include "ui/editors/MatchTestOverlay.h"

#include "core/capture/ScreenCapture.h"

#include <QApplication>
#include <QMessageBox>
#include <QString>
#include <QTimer>
#include <QWidget>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <functional>
#include <memory>
#include <string>

namespace {

#ifdef _WIN32
constexpr wchar_t kOverlayClassName[] = L"SbmMatchTestOverlay";
constexpr int kEscHotkeyId = 0x4d54;
constexpr UINT kMsgDismissOverlay = WM_APP + 0x4d54;

struct OverlayMatch {
    QRect clientRect;
    double confidence = 0.0;
};

struct OverlayState {
    HWND hwnd = nullptr;
    QRect physicalBounds;
    std::vector<OverlayMatch> matches;
    double threshold = 0.85;
    std::wstring hintText;
    MatchTestOverlay::VisibilityHandler onVisibilityChanged;
};

std::unique_ptr<OverlayState> g_state;
HHOOK g_keyboardHook = nullptr;

void uninstallKeyboardHook();
void notifyHidden();

LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (info->vkCode == VK_ESCAPE && g_state && g_state->hwnd) {
            PostMessageW(g_state->hwnd, kMsgDismissOverlay, 0, 0);
            return 1;
        }
    }
    return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

void installKeyboardHook() {
    if (!g_keyboardHook) {
        g_keyboardHook =
            SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc, GetModuleHandleW(nullptr), 0);
    }
}

void uninstallKeyboardHook() {
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
}

QWidget* messageBoxParent(QWidget* parent) {
    if (!parent) {
        return nullptr;
    }
    if (parent->isVisible()) {
        return parent;
    }
    if (QWidget* window = parent->window()) {
        return window;
    }
    return parent;
}

void notifyHidden() {
    MatchTestOverlay::VisibilityHandler callback;
    if (g_state) {
        callback = std::move(g_state->onVisibilityChanged);
    }
    if (callback) {
        QTimer::singleShot(0, qApp, [callback = std::move(callback)]() { callback(false); });
    }
}

void destroyOverlayWindow() {
    if (!g_state || !g_state->hwnd) {
        g_state.reset();
        uninstallKeyboardHook();
        return;
    }

    UnregisterHotKey(g_state->hwnd, kEscHotkeyId);
    DestroyWindow(g_state->hwnd);
    g_state->hwnd = nullptr;
    notifyHidden();
    g_state.reset();
    uninstallKeyboardHook();
}

std::vector<OverlayMatch> buildOverlayMatches(SearchArea searchArea,
                                              const CaptureRegion& customRegion,
                                              const PercentRegion& percentRegion,
                                              const std::vector<MatchResult>& matches,
                                              const QRect& targetPhysical) {
    std::vector<OverlayMatch> items;
    const QRect targetClient(0, 0, targetPhysical.width(), targetPhysical.height());

    if (searchArea == SearchArea::TargetWindow) {
        for (const MatchResult& match : matches) {
            if (!match.found || match.matchedSize.width <= 0 || match.matchedSize.height <= 0) {
                continue;
            }
            QRect clientRect(match.location.x,
                             match.location.y,
                             match.matchedSize.width,
                             match.matchedSize.height);
            clientRect = clientRect.intersected(targetClient);
            if (clientRect.width() >= 2 && clientRect.height() >= 2) {
                items.push_back({clientRect, match.confidence});
            }
        }
        return items;
    }

    QRect haystackPhysical;
    if (!ScreenCapture::searchAreaPhysicalRect(searchArea, customRegion, percentRegion, haystackPhysical)) {
        return items;
    }

    for (const MatchResult& match : matches) {
        if (!match.found || match.matchedSize.width <= 0 || match.matchedSize.height <= 0) {
            continue;
        }
        QRect matchPhysical(haystackPhysical.x() + match.location.x,
                            haystackPhysical.y() + match.location.y,
                            match.matchedSize.width,
                            match.matchedSize.height);
        QRect clientRect =
            matchPhysical.translated(-targetPhysical.x(), -targetPhysical.y()).intersected(targetClient);
        if (clientRect.width() >= 2 && clientRect.height() >= 2) {
            items.push_back({clientRect, match.confidence});
        }
    }
    return items;
}

void drawConfidenceLabel(HDC hdc, const OverlayMatch& match, int clientHeight) {
    wchar_t label[32]{};
    swprintf_s(label, L"%.2f", match.confidence);
    const int labelLen = static_cast<int>(wcslen(label));

    HFONT font = CreateFontW(-22,
                             0,
                             0,
                             0,
                             FW_BOLD,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE,
                             L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, font);

    SIZE textSize{};
    GetTextExtentPoint32W(hdc, label, labelLen, &textSize);

    constexpr int kPadX = 6;
    constexpr int kPadY = 4;
    constexpr int kGapAboveBox = 4;

    int textX = match.clientRect.left();
    int textY = match.clientRect.top() - textSize.cy - kPadY * 2 - kGapAboveBox;
    if (textY < 2) {
        textY = match.clientRect.top() + kGapAboveBox;
    }

    RECT bgRect{textX,
                textY,
                textX + textSize.cx + kPadX * 2,
                textY + textSize.cy + kPadY * 2};
    if (bgRect.bottom > clientHeight - 2) {
        bgRect.bottom = clientHeight - 2;
    }

    HBRUSH bgBrush = CreateSolidBrush(RGB(24, 24, 24));
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(80, 255, 120));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, bgRect.left, bgRect.top, bgRect.right, bgRect.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOutW(hdc, bgRect.left + kPadX, bgRect.top + kPadY, label, labelLen);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void paintOverlay(HDC hdc, const OverlayState& state) {
    RECT clientRect{};
    GetClientRect(state.hwnd, &clientRect);

    HBRUSH dimBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &clientRect, dimBrush);
    DeleteObject(dimBrush);

    for (const OverlayMatch& match : state.matches) {
        const QRect& roi = match.clientRect;
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(80, 255, 120));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, roi.left(), roi.top(), roi.right() + 1, roi.bottom() + 1);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        HBRUSH fillBrush = CreateSolidBrush(RGB(40, 180, 90));
        const RECT inner = {roi.left() + 1, roi.top() + 1, roi.right(), roi.bottom()};
        FillRect(hdc, &inner, fillBrush);
        DeleteObject(fillBrush);

        drawConfidenceLabel(hdc, match, clientRect.bottom);
    }

    if (!state.hintText.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT textRect{16, 16, clientRect.right - 16, clientRect.bottom - 16};
        DrawTextW(hdc, state.hintText.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
}

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    OverlayState* state = reinterpret_cast<OverlayState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<LPCREATESTRUCT>(lParam);
        state = static_cast<OverlayState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        SetLayeredWindowAttributes(hwnd, 0, 120, LWA_ALPHA);
        return 0;
    }
    case WM_PAINT: {
        if (!state) {
            break;
        }
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        paintOverlay(hdc, *state);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_HOTKEY:
        if (wParam == kEscHotkeyId) {
            MatchTestOverlay::dismissAll();
            return 0;
        }
        break;
    case kMsgDismissOverlay:
        MatchTestOverlay::dismissAll();
        return 0;
    case WM_DESTROY:
        UnregisterHotKey(hwnd, kEscHotkeyId);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ensureOverlayClassRegistered() {
    static const ATOM atom = []() -> ATOM {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = overlayWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kOverlayClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        return RegisterClassExW(&wc);
    }();
    return atom != 0;
}

bool createOverlayWindow(OverlayState& state) {
    if (!ensureOverlayClassRegistered()) {
        return false;
    }

    const int x = state.physicalBounds.x();
    const int y = state.physicalBounds.y();
    const int w = state.physicalBounds.width();
    const int h = state.physicalBounds.height();

    state.hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                                 kOverlayClassName,
                                 L"",
                                 WS_POPUP,
                                 x,
                                 y,
                                 w,
                                 h,
                                 nullptr,
                                 nullptr,
                                 GetModuleHandleW(nullptr),
                                 &state);

    if (!state.hwnd) {
        return false;
    }

    ShowWindow(state.hwnd, SW_SHOW);
    SetWindowPos(state.hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
    installKeyboardHook();
    if (!RegisterHotKey(state.hwnd, kEscHotkeyId, 0, VK_ESCAPE)) {
        // Low-level hook handles Esc when RegisterHotKey fails (e.g. game has focus).
    }
    InvalidateRect(state.hwnd, nullptr, TRUE);
    return true;
}

std::wstring buildHintText(size_t matchCount, double threshold) {
    if (matchCount == 0) {
        return QObject::tr("매칭 없음 (임계값 %1)\n매칭 테스트 끄기 또는 Esc로 닫기")
            .arg(QString::number(threshold, 'f', 2))
            .toStdWString();
    }
    return QObject::tr("매칭 %1개 · 매칭 테스트 끄기 또는 Esc로 닫기")
        .arg(static_cast<qlonglong>(matchCount))
        .toStdWString();
}

#endif // _WIN32

} // namespace

bool MatchTestOverlay::isVisible() {
#ifdef _WIN32
    return g_state && g_state->hwnd;
#else
    return false;
#endif
}

bool MatchTestOverlay::hide() {
#ifdef _WIN32
    if (!isVisible()) {
        return false;
    }
    dismissAll();
    return true;
#else
    return false;
#endif
}

bool MatchTestOverlay::show(const std::vector<MatchResult>& matches,
                            double threshold,
                            SearchArea searchArea,
                            const CaptureRegion& customRegion,
                            const PercentRegion& percentRegion,
                            QWidget* hostWidget,
                            VisibilityHandler onVisibilityChanged,
                            const std::vector<std::pair<CaptureRegion, std::vector<MatchResult>>>*
                                matchesPerCustomRegion) {
#ifdef _WIN32
    dismissAll();

    if (!ScreenCapture::findTargetWindow()) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("매칭 테스트"),
                             QObject::tr("대상 창을 찾을 수 없습니다.\n"
                                         "메인 창에서 먼저 '창 지정'을 사용하세요."));
        return false;
    }

    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (!target.valid || target.width <= 0 || target.height <= 0) {
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("매칭 테스트"),
                             QObject::tr("대상 창 영역을 확인할 수 없습니다."));
        return false;
    }

    const QRect targetPhysical(target.x, target.y, target.width, target.height);
    std::vector<OverlayMatch> overlayMatches;
    if (matchesPerCustomRegion && !matchesPerCustomRegion->empty()) {
        for (const auto& batch : *matchesPerCustomRegion) {
            const std::vector<OverlayMatch> regionItems = buildOverlayMatches(
                SearchArea::CustomRegion, batch.first, percentRegion, batch.second, targetPhysical);
            overlayMatches.insert(overlayMatches.end(), regionItems.begin(), regionItems.end());
        }
    } else {
        overlayMatches =
            buildOverlayMatches(searchArea, customRegion, percentRegion, matches, targetPhysical);
    }

    g_state = std::make_unique<OverlayState>();
    g_state->physicalBounds = targetPhysical;
    g_state->matches = std::move(overlayMatches);
    g_state->threshold = threshold;
    g_state->hintText = buildHintText(g_state->matches.size(), threshold);
    g_state->onVisibilityChanged = std::move(onVisibilityChanged);

    if (!createOverlayWindow(*g_state)) {
        g_state.reset();
        QMessageBox::warning(messageBoxParent(hostWidget),
                             QObject::tr("매칭 테스트"),
                             QObject::tr("매칭 결과 오버레이를 표시하지 못했습니다."));
        return false;
    }

    return true;
#else
    (void)matches;
    (void)threshold;
    (void)searchArea;
    (void)customRegion;
    (void)percentRegion;
    (void)hostWidget;
    (void)onVisibilityChanged;
    (void)matchesPerCustomRegion;
    return false;
#endif
}

void MatchTestOverlay::dismissAll() {
#ifdef _WIN32
    destroyOverlayWindow();
#endif
}
