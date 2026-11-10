#pragma once

#include <QIcon>
#include "model/ProfileListData.h"
#include <QString>
#include <QVector>
#include <functional>

class ProfileListWidget;
class QPushButton;

/// Renders value snapshots only. No profile persistence, capture or session access.
class ProfileListPresenter {
public:
    using Row=ProfileListRow;
    struct WindowIcon { QString title, processPath; QIcon icon; };
    struct IconAccess {
        std::function<QIcon(const QString&)> processIcon;
        std::function<QVector<WindowIcon>()> windows;
    };
    ProfileListPresenter(ProfileListWidget&, QPushButton* deleteButton, IconAccess);
    ProfileListPresenter(ProfileListWidget&, QPushButton* deleteButton);
    void refresh(const QVector<Row>& rows, const QString& activeId, const QIcon& fallback);
    void syncSelection(const QString& activeId);
private:
    ProfileListWidget& m_list;
    QPushButton* m_deleteButton;
    IconAccess m_icons;
};
