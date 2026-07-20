#pragma once

#include "ui/TargetWindowBindingRole.h"

#include <QIcon>
#include <QString>
#include <QVector>

#include <optional>
#include <string>

class QListWidgetItem;
class QPalette;
class QWidget;

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

struct TargetWindowListEntry {
#ifdef _WIN32
    HWND hwnd = nullptr;
#endif
    std::wstring title;
    QString processPath;
    QString displayText;
    QIcon icon;
};

struct TargetWindowListPickOptions {
    TargetWindowBindingRole role = TargetWindowBindingRole::Main;
    QString mainBinding;
    QString subBinding;
#ifdef _WIN32
    HWND preferredHwnd = nullptr;
#endif
};

struct TargetWindowListPickResult {
#ifdef _WIN32
    HWND hwnd = nullptr;
#endif
    QString title;
};

bool targetWindowTitleMatchesMainBinding(const QString& windowTitle, const QString& mainBinding);
bool targetWindowTitleMatchesSubTarget(const QString& windowTitle,
                                       const QString& mainBinding,
                                       const QString& subBinding);

void applyTargetWindowListBindingMark(QListWidgetItem* item,
                                      const QPalette& pal,
                                      bool isMainBinding,
                                      bool isSubBinding);
QString targetWindowListBindingLegendHtml(const QPalette& pal);

#ifdef _WIN32
QIcon targetWindowIconForProcessPath(const std::wstring& processPath);
QVector<TargetWindowListEntry> collectTargetWindowListEntries();

/// Modal picker shared by the main window and profile edit dialog.
/// Returns `std::nullopt` when cancelled or when no visible windows exist.
std::optional<TargetWindowListPickResult> pickTargetWindowFromList(QWidget* parent,
                                                                   const TargetWindowListPickOptions& options);
#endif
