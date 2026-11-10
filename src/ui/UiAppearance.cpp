#include "ui/UiAppearance.h"
#include <QApplication>
#include <QFont>
#include <QPainter>
#include <QPalette>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QWidget>

namespace {
class WorkspaceSurface final : public QWidget {
public:
    explicit WorkspaceSurface(QWidget* parent) : QWidget(parent) { setAttribute(Qt::WA_OpaquePaintEvent); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        QLinearGradient base(rect().topLeft(), rect().bottomRight());
        base.setColorAt(0, QColor("#24242d"));
        base.setColorAt(.48, QColor("#191d25"));
        base.setColorAt(1, QColor("#20292d"));
        painter.fillRect(rect(), base);
        const qreal span = qMax(width(), height());
        const auto ambient = [&](QPointF center, qreal radius, QColor tint) {
            QRadialGradient glow(center, qMax(qreal(1), radius));
            glow.setColorAt(0, tint);
            tint.setAlpha(0); // Fade the same hue, avoiding dark rims.
            glow.setColorAt(1, tint);
            painter.fillRect(rect(), glow);
        };
        ambient(QPointF(width() * .08, height() * .12), span * .68, QColor(174, 158, 198, 61));
        ambient(QPointF(width() * .60, -height() * .22), span * .58, QColor(215, 206, 186, 31));
        ambient(QPointF(width() * 1.02, height() * .76), span * .62, QColor(128, 181, 174, 41));
        ambient(QPointF(width() * .40, height() * 1.12), span * .48, QColor(132, 144, 182, 26));
    }
};
}

namespace UiAppearance {
void installPalette(QApplication& app) {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#24262e"));
    palette.setColor(QPalette::WindowText, QColor("#eeeef2"));
    palette.setColor(QPalette::Base, QColor("#292d37"));
    palette.setColor(QPalette::AlternateBase, QColor("#303440"));
    palette.setColor(QPalette::Text, QColor("#eeeef2"));
    palette.setColor(QPalette::Button, QColor("#3a3e4b"));
    palette.setColor(QPalette::ButtonText, QColor("#eeeef2"));
    palette.setColor(QPalette::Light, QColor("#414b60"));
    palette.setColor(QPalette::Midlight, QColor("#353e50"));
    palette.setColor(QPalette::Mid, QColor("#495263"));
    palette.setColor(QPalette::Dark, QColor("#161c27"));
    palette.setColor(QPalette::Shadow, QColor("#0f141e"));
    palette.setColor(QPalette::Highlight, QColor("#747b94"));
    palette.setColor(QPalette::HighlightedText, QColor("#f5f6fc"));
    palette.setColor(QPalette::Link, QColor("#b7c9dc"));
    palette.setColor(QPalette::PlaceholderText, QColor("#a0a5b5"));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, QColor("#7e899c"));
    app.setPalette(palette);
    app.setFont(QFont(QStringLiteral("Malgun Gothic"), 9));
}

QWidget* createWorkspaceSurface(QWidget* parent) { return new WorkspaceSurface(parent); }

QString glassPanelStyle(const QString& selector) {
    return selector + QStringLiteral(R"( {
        background: qlineargradient(x1:0,y1:0,x2:1,y2:1,
            stop:0 rgba(126,122,147,49),stop:0.38 rgba(75,82,99,40),stop:1 rgba(81,112,113,34));
        border: 1px solid rgba(226,231,246,28);
        border-top-color: rgba(246,241,255,63);
        border-left-color: rgba(236,232,251,39);
        border-radius: 13px;
    })");
}

QString styleSheet() {
    return glassPanelStyle(QStringLiteral("QFrame#blockListTableFrame")) + QStringLiteral(R"(
QMainWindow, QDialog { background: #24262e; }
QPushButton, QToolButton {
    background: qlineargradient(x1:0,y1:0,x2:0.8,y2:1,stop:0 rgba(193,185,212,44),stop:0.48 rgba(128,133,156,29),stop:1 rgba(120,154,151,24));
    border: 1px solid rgba(230,230,248,29); border-top-color: rgba(250,244,255,65);
    border-radius: 8px; padding: 4px 9px; color: #e9e9f0;
}
QPushButton:hover:!pressed:!disabled, QToolButton:hover:!pressed:!disabled:!checked {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 rgba(203,192,223,72),stop:0.5 rgba(150,155,182,49),stop:1 rgba(148,187,177,44));
    border-color: rgba(232,231,249,80); border-top-color: rgba(255,249,255,115);
}
QPushButton:pressed:!disabled, QToolButton:pressed:!disabled { background: rgba(126,133,157,45); border-color: #929aae; }
QPushButton:disabled, QToolButton:disabled { color: #9399a9; background: rgba(132,136,157,14); border-color: rgba(218,228,248,13); }
QPushButton:focus, QToolButton:focus { border-color: #c3c3dc; }
QPushButton:checked, QToolButton:checked { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #676879,stop:1 #4c666c); color: #fafbff; }
QLabel { background: transparent; }
QGroupBox { border: none; font-weight: 600; }
QGroupBox::title { color: #c7cedf; }
QTableWidget { background: transparent; border: none; selection-background-color: #626b80; selection-color: #f7f9ff; }
QLineEdit, QComboBox, QAbstractSpinBox, QPlainTextEdit {
    background: rgba(18,21,29,105); color: #eeeef4; border: 1px solid rgba(222,221,241,35);
    border-radius: 7px; padding: 4px; selection-background-color: #626b80;
}
QLineEdit:focus, QAbstractSpinBox:focus { border-color: #a5b4d0; }
QCheckBox, QRadioButton { spacing: 6px; color: #c9d1e2; background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle { background: rgba(177,194,220,65); border-radius: 3px; min-width: 22px; min-height: 22px; }
QScrollBar::handle:hover { background: rgba(193,210,236,115); }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QMenu { background: #30323e; color: #eeeef4; border: 1px solid #5b5c70; border-radius: 9px; padding: 6px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 5px; }
QMenu::item:selected { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #626276,stop:1 #4a6268); color: #fff; }
QMenu::separator { height: 1px; margin: 5px 10px; background: #414b5e; }
QToolTip { color: #eef1f7; background: #343d50; border: 1px solid #65738c; padding: 6px; }
QTabBar::tab { background: #293243; padding: 7px 13px; border-radius: 6px; margin: 2px; }
QTabBar::tab:selected { background: #53627c; }
)");
}
}
