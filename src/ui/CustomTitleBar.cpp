#include "ui/CustomTitleBar.h"

#include "ui/TitleBarIconEasterEgg.h"

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QApplication>
#include <QCheckBox>

namespace {

constexpr int kTitleBarAppBadgeSize = 30;

QPushButton* makeTitleBarButton(const QString& text, const char* objectName, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName(QString::fromLatin1(objectName));
    button->setFlat(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(40, 32);
    return button;
}

} // namespace

CustomTitleBar::CustomTitleBar(QMainWindow* window)
    : m_window(window) {
    setObjectName(QStringLiteral("customTitleBar"));
    setFixedHeight(42);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true);

    setupUi();

    if (m_window) {
        m_window->installEventFilter(this);
        syncFromWindowTitle();
    }
}

void CustomTitleBar::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 5, 0, 5);
    layout->setSpacing(6);

    m_appBadge = new QLabel(this);
    m_appBadge->setObjectName(QStringLiteral("titleBarAppBadge"));
    m_appBadge->setAlignment(Qt::AlignCenter);
    m_appBadge->setFixedSize(kTitleBarAppBadgeSize, kTitleBarAppBadgeSize);
    m_appBadge->setScaledContents(true);
    m_appBadge->setCursor(Qt::PointingHandCursor);
    m_appBadge->setToolTip(tr("PIPBONG"));
    const QIcon appIcon = QApplication::windowIcon();
    if (!appIcon.isNull()) {
        m_appBadge->setPixmap(appIcon.pixmap(kTitleBarAppBadgeSize, kTitleBarAppBadgeSize));
    } else {
        m_appBadge->setText(QStringLiteral("PIP"));
    }

    m_menuBar = new QMenuBar(this);
    m_menuBar->setObjectName(QStringLiteral("titleBarMenuBar"));
    m_menuBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    m_dragRegion = new QWidget(this);
    m_dragRegion->setObjectName(QStringLiteral("titleBarDragRegion"));
    m_dragRegion->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_dragRegion->setAttribute(Qt::WA_StyledBackground, true);

    auto* dragLayout = new QHBoxLayout(m_dragRegion);
    dragLayout->setContentsMargins(8, 0, 8, 0);
    dragLayout->setSpacing(0);

    m_titleLabel = new QLabel(m_dragRegion);
    m_titleLabel->setObjectName(QStringLiteral("titleBarTitle"));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_titleLabel->setMinimumWidth(48);
    dragLayout->addWidget(m_titleLabel, 1);

    m_dragRegion->installEventFilter(this);
    m_appBadge->installEventFilter(this);

    m_minimizeButton = makeTitleBarButton(QStringLiteral("\u2212"), "titleBarMinimizeButton", this);
    m_maximizeButton = makeTitleBarButton(QStringLiteral("\u25a1"), "titleBarMaximizeButton", this);
    m_closeButton = makeTitleBarButton(QStringLiteral("\u00d7"), "titleBarCloseButton", this);

    layout->addWidget(m_appBadge, 0, Qt::AlignVCenter);
    layout->addWidget(m_menuBar, 0, Qt::AlignVCenter);
    layout->addWidget(m_dragRegion, 1);
    layout->addWidget(m_minimizeButton, 0, Qt::AlignVCenter);
    layout->addWidget(m_maximizeButton, 0, Qt::AlignVCenter);
    layout->addWidget(m_closeButton, 0, Qt::AlignVCenter);

    connect(m_minimizeButton, &QPushButton::clicked, this, [this]() {
        if (m_window) {
            m_window->showMinimized();
        }
    });
    connect(m_maximizeButton, &QPushButton::clicked, this, [this]() { toggleMaximize(); });
    connect(m_closeButton, &QPushButton::clicked, m_window, &QMainWindow::close);

    // Apply once in ctor only. Do not call setStyleSheet from changeEvent (StyleChange reentrancy
    // causes stack overflow 0xC00000FD). See AGENTS.md §8.4 and qt-changeevent-stylesheet.mdc.
    setStyleSheet(QStringLiteral(
        "QWidget#customTitleBar {"
        "  background-color: palette(window);"
        "  border-bottom: 1px solid palette(mid);"
        "}"
        "QLabel#titleBarAppBadge {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0;"
        "}"
        "QWidget#titleBarDragRegion {"
        "  background: transparent;"
        "}"
        "QLabel#titleBarTitle {"
        "  color: palette(window-text);"
        "  background: transparent;"
        "  font-size: 12px;"
        "}"
        "QMenuBar#titleBarMenuBar {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 2px 0 0 0;"
        "  spacing: 2px;"
        "}"
        "QMenuBar#titleBarMenuBar::item {"
        "  background: transparent;"
        "  padding: 5px 10px 3px 10px;"
        "  border-radius: 6px;"
        "}"
        "QMenuBar#titleBarMenuBar::item:selected {"
        "  background-color: palette(button);"
        "}"
        "QCheckBox#titleBarAlwaysOnTopCheck {"
        "  spacing: 4px;"
        "  padding: 0 8px 1px 8px;"
        "  color: palette(window-text);"
        "}"
        "QCheckBox#titleBarAlwaysOnTopCheck::indicator {"
        "  width: 14px;"
        "  height: 14px;"
        "}"
        "QPushButton#titleBarMinimizeButton,"
        "QPushButton#titleBarMaximizeButton,"
        "QPushButton#titleBarCloseButton {"
        "  border: none;"
        "  border-radius: 6px;"
        "  background: transparent;"
        "  color: palette(window-text);"
        "  font-size: 14px;"
        "  padding: 0;"
        "}"
        "QPushButton#titleBarMinimizeButton:hover,"
        "QPushButton#titleBarMaximizeButton:hover {"
        "  background-color: palette(button);"
        "}"
        "QPushButton#titleBarCloseButton:hover {"
        "  background-color: #e81123;"
        "  color: white;"
        "}"
        "QPushButton#titleBarCloseButton:pressed {"
        "  background-color: #bf360c;"
        "  color: white;"
        "}"));
}

void CustomTitleBar::setAlwaysOnTopCheckBox(QCheckBox* checkBox) {
    if (!checkBox || m_alwaysOnTopCheck || !m_minimizeButton) {
        return;
    }
    m_alwaysOnTopCheck = checkBox;
    checkBox->setParent(this);
    checkBox->setObjectName(QStringLiteral("titleBarAlwaysOnTopCheck"));
    checkBox->setFocusPolicy(Qt::NoFocus);
    if (auto* rowLayout = qobject_cast<QHBoxLayout*>(layout())) {
        const int minimizeIndex = rowLayout->indexOf(m_minimizeButton);
        rowLayout->insertWidget(minimizeIndex, checkBox, 0, Qt::AlignVCenter);
    }
}

void CustomTitleBar::syncFromWindowTitle() {
    if (!m_window) {
        return;
    }
    m_fullTitle = m_window->windowTitle();
    updateTitleElide();
}

void CustomTitleBar::updateTitleElide() {
    if (!m_titleLabel) {
        return;
    }

    QPalette labelPal = m_titleLabel->palette();
    labelPal.setColor(QPalette::WindowText, palette().color(QPalette::WindowText));
    labelPal.setColor(QPalette::Text, palette().color(QPalette::WindowText));
    m_titleLabel->setPalette(labelPal);

    if (m_fullTitle.isEmpty()) {
        m_titleLabel->clear();
        m_titleLabel->setToolTip({});
        return;
    }

    const QFontMetrics metrics(m_titleLabel->fontMetrics());
    int available = m_titleLabel->width();
    if (available <= 0 && m_dragRegion) {
        available = m_dragRegion->width() - 16;
    }
    if (available <= 0) {
        available = qMax(width() / 2, 160);
    }
    available = qMax(available, 48);
    m_titleLabel->setText(metrics.elidedText(m_fullTitle, Qt::ElideRight, available));
    m_titleLabel->setToolTip(m_fullTitle);
}

void CustomTitleBar::updateMaximizeButton() {
    if (!m_maximizeButton || !m_window) {
        return;
    }
    const bool maximized = m_window->isMaximized();
    m_maximizeButton->setText(maximized ? QStringLiteral("\u29c9") : QStringLiteral("\u25a1"));
    m_maximizeButton->setToolTip(maximized ? tr("복원") : tr("최대화"));
}

void CustomTitleBar::startSystemMove(const QPoint& globalPos) {
    if (!m_window) {
        return;
    }
    if (m_window->isMaximized()) {
        const double widthRatio =
            static_cast<double>(globalPos.x() - m_window->frameGeometry().left())
            / static_cast<double>(qMax(m_window->frameGeometry().width(), 1));
        m_window->showNormal();
        const int restoredX = globalPos.x() - static_cast<int>(m_window->width() * widthRatio);
        m_window->move(restoredX, globalPos.y() - m_dragOffset.y());
    }
    m_dragging = true;
    m_dragOffset = globalPos - m_window->frameGeometry().topLeft();
}

void CustomTitleBar::toggleMaximize() {
    if (!m_window) {
        return;
    }
    if (m_window->isMaximized()) {
        m_window->showNormal();
    } else {
        m_window->showMaximized();
    }
    updateMaximizeButton();
}

void CustomTitleBar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    syncFromWindowTitle();
}

void CustomTitleBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateTitleElide();
}

bool CustomTitleBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_window) {
        if (event->type() == QEvent::WindowStateChange) {
            updateMaximizeButton();
            return false;
        }
        if (event->type() == QEvent::WindowTitleChange) {
            syncFromWindowTitle();
            return false;
        }
    }

    if (watched == m_dragRegion && event->type() == QEvent::Resize) {
        updateTitleElide();
        return false;
    }

    if (watched == m_appBadge && event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            TitleBarIconEasterEgg::play(QApplication::windowIcon(), m_appBadge);
            return true;
        }
    }

    if (watched == m_dragRegion && m_window) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                startSystemMove(mouseEvent->globalPosition().toPoint());
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (!m_dragging) {
                break;
            }
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                m_window->move(mouseEvent->globalPosition().toPoint() - m_dragOffset);
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragging = false;
                return true;
            }
            break;
        }
        case QEvent::MouseButtonDblClick: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                toggleMaximize();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}
