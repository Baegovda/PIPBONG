#include "ui/UiStateManager.h"
#include "ui/UiResizeHandle.h"

#include <QEvent>
#include <QHeaderView>
#include <QMainWindow>
#include <QSettings>
#include <QSplitter>
#include <QTimer>

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
        const QByteArray geometry = settings.value(settingsKey(m_mainWindowId + QStringLiteral("/geometry"))).toByteArray();
        if (!geometry.isEmpty()) {
            m_mainWindow->restoreGeometry(geometry);
        }
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

void UiStateManager::saveNow() {
    if (m_restoring) {
        return;
    }

    QSettings settings;
    if (m_mainWindow) {
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
