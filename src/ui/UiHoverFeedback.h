#pragma once

#include "ui/UiResizeHandle.h"

#include <QColor>
#include <QPalette>
#include <QString>

/// Program-wide interactive hover feedback (buttons, inputs, list/table rows).
/// Apply the stylesheet once from Application ctor — never from StyleChange.
namespace UiHoverFeedback {

inline QColor listRowHoverOverlay(const QPalette& pal) {
    QColor tint = pal.color(QPalette::Highlight);
    if (!tint.isValid()) {
        tint = pal.color(QPalette::Mid);
    }
    const QColor base = pal.color(QPalette::Base);
    const bool dark = base.isValid() ? base.lightness() < 128 : tint.lightness() > 128;
    tint.setAlpha(dark ? 48 : 36);
    return tint;
}

inline QColor blendListRowHover(const QPalette& pal) {
    const QColor base = pal.color(QPalette::Base);
    const QColor overlay = listRowHoverOverlay(pal);
    if (!base.isValid()) {
        return overlay;
    }
    const int oa = overlay.alpha();
    const int ia = 255 - oa;
    return QColor((base.red() * ia + overlay.red() * oa) / 255,
                  (base.green() * ia + overlay.green() * oa) / 255,
                  (base.blue() * ia + overlay.blue() * oa) / 255);
}

/// Soft highlight for card-style list rows (profile / library).
inline QColor cardRowHoverFill(const QPalette& pal) {
    QColor fill = pal.color(QPalette::Button).isValid() ? pal.color(QPalette::Button)
                                                        : pal.color(QPalette::AlternateBase);
    QColor tint = pal.color(QPalette::Highlight);
    if (!tint.isValid()) {
        return fill;
    }
    tint.setAlpha(40);
    const int oa = tint.alpha();
    const int ia = 255 - oa;
    return QColor((fill.red() * ia + tint.red() * oa) / 255,
                  (fill.green() * ia + tint.green() * oa) / 255,
                  (fill.blue() * ia + tint.blue() * oa) / 255);
}

/// Application-wide stylesheet for stock interactive controls (palette-based).
/// Widgets with their own setStyleSheet keep their local rules; local :hover still wins when defined.
inline QString programWideStyleSheet() {
    return QStringLiteral(
        // Buttons
        "QPushButton:hover:!pressed:!disabled {"
        "  background-color: palette(light);"
        "}"
        "QPushButton:pressed:!disabled {"
        "  background-color: palette(midlight);"
        "}"
        "QToolButton:hover:!pressed:!disabled:!checked {"
        "  background-color: palette(light);"
        "}"
        "QToolButton:hover:!pressed:!disabled:checked {"
        "  background-color: palette(midlight);"
        "}"
        // Check / radio
        "QCheckBox:hover:!disabled,"
        "QRadioButton:hover:!disabled {"
        "  color: palette(window-text);"
        "}"
        // Combo / edits / spin
        "QComboBox:hover:!disabled,"
        "QLineEdit:hover:!disabled,"
        "QTextEdit:hover:!disabled,"
        "QPlainTextEdit:hover:!disabled,"
        "QAbstractSpinBox:hover:!disabled {"
        "  border: 1px solid palette(highlight);"
        "}"
        // Tables / lists that use the style item panel
        "QTableWidget::item:hover,"
        "QTableView::item:hover,"
        "QTreeView::item:hover,"
        "QListWidget::item:hover,"
        "QListView::item:hover {"
        "  background-color: palette(midlight);"
        "}"
        // Header sections (column dividers / section hover)
        "QHeaderView::section:hover {"
        "  background-color: palette(light);"
        "}"
        // Splitter handles — base width/height required when a global stylesheet is set;
        // :hover-only rules collapse handles and break drag resize (setHandleWidth ignored).
        "QSplitter::handle {"
        "  background-color: palette(mid);"
        "  margin: 0;"
        "  padding: 0;"
        "}"
        "QSplitter::handle:horizontal {"
        "  width: %1px;"
        "}"
        "QSplitter::handle:vertical {"
        "  height: %1px;"
        "}"
        "QSplitter::handle:hover {"
        "  background-color: palette(highlight);"
        "}"
        // Scrollbar grip
        "QScrollBar::handle:hover {"
        "  background: palette(dark);"
        "}"
        // Tabs
        "QTabBar::tab:hover:!selected {"
        "  background-color: palette(light);"
        "}"
        // Menus
        "QMenu::item:selected {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
        "QMenuBar::item:selected {"
        "  background-color: palette(button);"
        "}"
        // Dialog button box inherits QPushButton rules
        )
        .arg(UiResizeHandle::kSplitterHandleWidthPx);
}

} // namespace UiHoverFeedback
