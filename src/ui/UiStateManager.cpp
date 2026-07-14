#include "ui/UiStateManager.h"
#include "ui/UiResizeHandle.h"

#include <QEvent>
#include <QGuiApplication>
#include <QHeaderView>
#include <QMainWindow>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QTimer>
#include <QWindow>

namespace {

QString mainWindowPlacementKey(const QString& suffix) {
    return UiStateManager::settingsKey(QStringLiteral("mainWindow/placement/%1").arg(suffix));
}

QRect clampWindowRectToScreen(const QRect& rect, QScreen* screen) {
    if (!screen || !rect.isValid()) {
        return rect;
    }
    const QRect avail = screen->availableGeometry();
    QRect clamped = rect;
    if (clamped.width() > avail.width()) {
        clamped.setWidth(avail.width());
    }
    if (clamped.height() > avail.height()) {
        clamped.setHeight(avail.height());
    }
    if (clamped.right() > avail.right()) {
        clamped.moveRight(avail.right());
    }
    if (clamped.bottom() > avail.bottom()) {
        clamped.moveBottom(avail.bottom());
    }
    if (clamped.left() < avail.left()) {
        clamped.moveLeft(avail.left());
    }
    if (clamped.top() < avail.top()) {
        clamped.moveTop(avail.top());
    }
    return clamped;
}

QScreen* screenByName(const QString& screenName) {
    if (screenName.isEmpty()) {
        return nullptr;
    }
    for (QScreen* screen : QGuiApplication::screens()) {
        if (screen && screen->name() == screenName) {
            return screen;
        }
    }
    return nullptr;
}

void assignMainWindowToScreen(QMainWindow* window, QScreen* screen) {
    if (!window || !screen) {
        return;
    }
    if (QWindow* handle = window->windowHandle()) {
        handle->setScreen(screen);
    }
}

void saveMainWindowPlacement(QMainWindow* window, QSettings& settings) {
    if (!window) {
        return;
    }

    const QRect geometry =
        window->isMaximized() ? window->normalGeometry() : window->frameGeometry();
    QScreen* screen = window->screen();
    if (!screen && window->windowHandle()) {
        screen = window->windowHandle()->screen();
    }

    settings.setValue(mainWindowPlacementKey(QStringLiteral("screenName")),
                     screen ? screen->name() : QString());
    settings.setValue(mainWindowPlacementKey(QStringLiteral("x")), geometry.x());
    settings.setValue(mainWindowPlacementKey(QStringLiteral("y")), geometry.y());
    settings.setValue(mainWindowPlacementKey(QStringLiteral("width")), geometry.width());
    settings.setValue(mainWindowPlacementKey(QStringLiteral("height")), geometry.height());
    settings.setValue(mainWindowPlacementKey(QStringLiteral("maximized")), window->isMaximized());
}

bool restoreMainWindowPlacement(QMainWindow* window, const QSettings& settings) {
    if (!window) {
        return false;
    }

    const QString widthKey = mainWindowPlacementKey(QStringLiteral("width"));
    if (!settings.contains(widthKey)) {
        return false;
    }

    const QString screenName = settings.value(mainWindowPlacementKey(QStringLiteral("screenName"))).toString();
    const int x = settings.value(mainWindowPlacementKey(QStringLiteral("x"))).toInt();
    const int y = settings.value(mainWindowPlacementKey(QStringLiteral("y"))).toInt();
    const int width = settings.value(mainWindowPlacementKey(QStringLiteral("width"))).toInt();
    const int height = settings.value(mainWindowPlacementKey(QStringLiteral("height"))).toInt();
    const bool maximized = settings.value(mainWindowPlacementKey(QStringLiteral("maximized")), false).toBool();
    if (width <= 0 || height <= 0) {
        return false;
    }

    QScreen* screen = screenByName(screenName);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return false;
    }

    assignMainWindowToScreen(window, screen);
    QRect geometry(x, y, width, height);
    geometry = clampWindowRectToScreen(geometry, screen);

    if (maximized) {
        window->showNormal();
        window->setGeometry(geometry);
        window->showMaximized();
    } else {
        window->setGeometry(geometry);
    }
    return true;
}

void centerMainWindowOnPrimaryScreen(QMainWindow* window) {
    if (!window) {
        return;
    }
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }
    assignMainWindowToScreen(window, screen);
    QSize size = window->size();
    if (size.isEmpty()) {
        size = QSize(1280, 820);
    }
    const QRect avail = screen->availableGeometry();
    const int x = avail.x() + qMax(0, (avail.width() - size.width()) / 2);
    const int y = avail.y() + qMax(0, (avail.height() - size.height()) / 2);
    window->setGeometry(clampWindowRectToScreen(QRect(x, y, size.width(), size.height()), screen));
}

} // namespace

UiStateManager::UiStateManager(QObject* parent)
    : QObject(parent) {
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(500);
    connect(m_saveTimer, &QTimer::timeout, this, &UiStateManager::saveNow);
}

QString UiStateManager::settingsKey(const QString& id) {
    return QStringLiteral("ui/state/%1").arg(id);
}

void UiStateManager::registerMainWindow(QMainWindow* window, const QString& id) {
    m_mainWindow = window;
    m_mainWindowId = id;
    if (window) {
        window->installEventFilter(this);
    }
}

void UiStateManager::registerSplitter(QSplitter* splitter, const QString& id) {
    if (!splitter) {
        return;
    }
    splitter->setHandleWidth(UiResizeHandle::kSplitterHandleWidthPx);
    m_splitters.append({splitter, id});
    connect(splitter, &QSplitter::splitterMoved, this, &UiStateManager::scheduleSave);
}

void UiStateManager::registerHeader(QHeaderView* header, const QString& id) {
    if (!header) {
        return;
    }
    m_headers.append({header, id});
    connect(header, &QHeaderView::sectionResized, this, &UiStateManager::scheduleSave);
    connect(header, &QHeaderView::sectionMoved, this, &UiStateManager::scheduleSave);
}

void UiStateManager::registerSettingsHooks(const QString& id,
                                           const std::function<void(QSettings&)>& writer,
                                           const std::function<void(const QSettings&)>& reader) {
    if (id.isEmpty() || !writer || !reader) {
        return;
    }
    m_settingsHooks.append({id, writer, reader});
}

void UiStateManager::restoreAll() {
    m_restoring = true;

    QSettings settings;
    if (m_mainWindow) {
        const QByteArray windowState =
            settings.value(settingsKey(m_mainWindowId + QStringLiteral("/windowState"))).toByteArray();
        if (!windowState.isEmpty()) {
            m_mainWindow->restoreState(windowState);
        }
    }

    for (const auto& entry : m_splitters) {
        const QByteArray state = settings.value(settingsKey(entry.second)).toByteArray();
        if (!state.isEmpty() && entry.first) {
            entry.first->restoreState(state);
        }
    }

    for (const auto& entry : m_headers) {
        const QByteArray state = settings.value(settingsKey(entry.second)).toByteArray();
        if (!state.isEmpty() && entry.first) {
            entry.first->restoreState(state);
        }
    }

    for (const auto& hook : m_settingsHooks) {
        hook.reader(settings);
    }

    m_restoring = false;
}

void UiStateManager::restoreMainWindowGeometry() {
    if (m_mainWindowGeometryRestored || !m_mainWindow) {
        return;
    }
    m_mainWindowGeometryRestored = true;

    m_restoring = true;
    QSettings settings;
    bool restored = restoreMainWindowPlacement(m_mainWindow, settings);
    if (!restored) {
        const QByteArray geometry =
            settings.value(settingsKey(m_mainWindowId + QStringLiteral("/geometry"))).toByteArray();
        if (!geometry.isEmpty()) {
            m_mainWindow->restoreGeometry(geometry);
            restored = true;
        }
    }
    if (!restored) {
        centerMainWindowOnPrimaryScreen(m_mainWindow);
    }
    m_restoring = false;
}

void UiStateManager::saveNow() {
    if (m_restoring) {
        return;
    }

    QSettings settings;
    if (m_mainWindow) {
        saveMainWindowPlacement(m_mainWindow, settings);
        settings.setValue(settingsKey(m_mainWindowId + QStringLiteral("/geometry")), m_mainWindow->saveGeometry());
        settings.setValue(settingsKey(m_mainWindowId + QStringLiteral("/windowState")), m_mainWindow->saveState());
    }

    for (const auto& entry : m_splitters) {
        if (entry.first) {
            settings.setValue(settingsKey(entry.second), entry.first->saveState());
        }
    }

    for (const auto& entry : m_headers) {
        if (entry.first) {
            settings.setValue(settingsKey(entry.second), entry.first->saveState());
        }
    }

    for (const auto& hook : m_settingsHooks) {
        hook.writer(settings);
    }

    settings.sync();
}

void UiStateManager::scheduleSave() {
    if (m_restoring) {
        return;
    }
    m_saveTimer->start();
}

bool UiStateManager::eventFilter(QObject* watched, QEvent* event) {
    if (!m_restoring && watched == m_mainWindow) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Move:
        case QEvent::WindowStateChange:
            scheduleSave();
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}
