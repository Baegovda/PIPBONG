#include "ui/ProfileListPresenter.h"
#include "ui/ProfileListWidget.h"
#include "ui/TargetWindowListPicker.h"
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <optional>

namespace {
QString identity(const QString& path) {
    return path.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(path)).toCaseFolded();
}
ProfileListPresenter::IconAccess nativeIcons() {
    ProfileListPresenter::IconAccess access;
#ifdef _WIN32
    access.processIcon = [](const QString& path) {
        return QFileInfo::exists(path) ? targetWindowIconForProcessPath(path.toStdWString()) : QIcon{};
    };
    access.windows = [] {
        QVector<ProfileListPresenter::WindowIcon> icons;
        for (const auto& window : collectTargetWindowListEntries())
            icons.push_back({QString::fromStdWString(window.title), window.processPath, window.icon});
        return icons;
    };
#endif
    return access;
}
}

ProfileListPresenter::ProfileListPresenter(ProfileListWidget& list, QPushButton* button, IconAccess icons)
    : m_list(list), m_deleteButton(button), m_icons(std::move(icons)) {}

ProfileListPresenter::ProfileListPresenter(ProfileListWidget& list, QPushButton* button)
    : ProfileListPresenter(list, button, nativeIcons()) {}

void ProfileListPresenter::refresh(const QVector<Row>& rows, const QString& activeId, const QIcon& fallback) {
    const bool sameSelection = m_list.currentItem()
        && m_list.currentItem()->data(Qt::UserRole).toString() == activeId;
    const int scroll = m_list.verticalScrollBar()->value();
    // clear() also emits selection changes; block the entire rebuild, not just setCurrentRow().
    const QSignalBlocker blocker(&m_list);
    m_list.clear();
    QString defaultId;
    for (const auto& row : rows) if (row.isDefault) { defaultId = row.id; break; }
    m_list.setDefaultProfileId(defaultId);
    std::optional<QVector<WindowIcon>> windows;
    const auto resolveIcon = [&](const Row& row) -> QIcon {
        if (row.isDefault) return fallback;
        if (!row.cachedIcon.isNull()) return row.cachedIcon;
        if (!row.processPath.isEmpty() && m_icons.processIcon) {
            const QIcon icon = m_icons.processIcon(row.processPath);
            if (!icon.isNull()) return icon;
        }
        if (row.targetTitle.isEmpty() || !m_icons.windows) return fallback;
        if (!windows) windows = m_icons.windows();
        const QString expectedPath = identity(row.processPath);
        QIcon candidate;
        QString candidatePath;
        for (const auto& window : *windows) {
            if (!window.title.contains(row.targetTitle, Qt::CaseInsensitive)) continue;
            const QString path = identity(window.processPath);
            if (path.isEmpty() || (!expectedPath.isEmpty() && path != expectedPath)) continue;
            // A title-only profile must not inherit an arbitrary program's icon.
            if (!candidatePath.isEmpty() && candidatePath != path) return fallback;
            candidatePath = path;
            if (!window.icon.isNull()) candidate = window.icon;
        }
        return candidate.isNull() ? fallback : candidate;
    };
    int activeRow = -1;
    bool canDelete = false;
    for (int i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        const QString name = row.isDefault ? QStringLiteral("기본") : row.name;
        auto* item = new QListWidgetItem(resolveIcon(row), name, &m_list);
        item->setData(Qt::UserRole, row.id);
        item->setData(Qt::UserRole + 2, row.isDefault);
        if (row.isDefault) item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
        QString detail;
        if (row.isDefault) detail = QObject::tr("시스템 기본 프로필\n이름·순서 변경 불가 · 타겟 미지정 · 연결된 프로그램이 없을 때 자동 선택");
        else if (row.targetTitle.isEmpty()) detail = QObject::tr("타겟 없음");
        else {
            const QString process = QFileInfo(QDir::fromNativeSeparators(row.processPath)).fileName();
            detail = process.isEmpty() ? QObject::tr("타겟: %1").arg(row.targetTitle)
                                      : QObject::tr("타겟: %1\n프로세스: %2").arg(row.targetTitle, process);
        }
        item->setToolTip(name + QLatin1Char('\n') + detail);
        if (row.id == activeId) { activeRow = i; canDelete = !row.isDefault; }
    }
    m_list.setCurrentRow(activeRow);
    if (sameSelection) {
        m_list.doItemsLayout();
        m_list.verticalScrollBar()->setValue(scroll);
    }
    if (m_deleteButton) m_deleteButton->setEnabled(rows.size() > 1 && canDelete);
}

void ProfileListPresenter::syncSelection(const QString& activeId) {
    const QSignalBlocker blocker(&m_list);
    for (int row = 0; row < m_list.count(); ++row) {
        if (m_list.item(row)->data(Qt::UserRole).toString() == activeId) {
            m_list.setCurrentRow(row);
            return;
        }
    }
    m_list.setCurrentRow(-1);
}
