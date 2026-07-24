#pragma once

#include <QCheckBox>
#include <QGroupBox>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

/// Shared helpers for settings / edit dialogs — category QGroupBox + tooltips.
namespace UiSettingsLayout {

inline void applyOptionToolTip(QWidget* widget, const QString& toolTip) {
    if (widget && !toolTip.isEmpty()) {
        widget->setToolTip(toolTip);
    }
}

inline QGroupBox* addSettingsGroup(QVBoxLayout* rootLayout,
                                   const QString& title,
                                   const QString& groupToolTip = QString()) {
    auto* group = new QGroupBox(title);
    applyOptionToolTip(group, groupToolTip);
    rootLayout->addWidget(group);
    return group;
}

inline QCheckBox* addGroupCheck(QVBoxLayout* groupLayout,
                                QWidget* parent,
                                const QString& label,
                                const QString& toolTip,
                                bool checked) {
    auto* check = new QCheckBox(label, parent);
    check->setChecked(checked);
    applyOptionToolTip(check, toolTip);
    groupLayout->addWidget(check);
    return check;
}

} // namespace
