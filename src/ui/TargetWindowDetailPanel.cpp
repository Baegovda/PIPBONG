#include "ui/TargetWindowDetailPanel.h"

#include "ui/UiThemeColors.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace {

QString htmlEscape(const QString& text) {
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return escaped;
}

QString captionSpan(const QString& caption, const QColor& muted) {
    return QStringLiteral("<span style=\"color:%1;\">%2</span>")
        .arg(muted.name(), htmlEscape(caption));
}

QString valueSpan(const QString& value, const QColor& text) {
    return QStringLiteral("<span style=\"color:%1;\">%2</span>")
        .arg(text.name(), htmlEscape(value));
}

} // namespace

TargetWindowDetailPanel::TargetWindowDetailPanel(QWidget* parent)
    : QFrame(parent) {
    setObjectName(QStringLiteral("targetWindowDetailPanel"));
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* stackHost = new QWidget(this);
    auto* stackedLayout = new QStackedLayout(stackHost);
    stackedLayout->setContentsMargins(0, 0, 0, 0);

    m_messagePage = new QWidget(stackHost);
    auto* messageLayout = new QVBoxLayout(m_messagePage);
    messageLayout->setContentsMargins(10, 8, 10, 8);
    m_messageLabel = new QLabel(m_messagePage);
    m_messageLabel->setObjectName(QStringLiteral("targetWindowDetailMessage"));
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    messageLayout->addWidget(m_messageLabel);

    m_detailsPage = new QWidget(stackHost);
    auto* detailsLayout = new QVBoxLayout(m_detailsPage);
    detailsLayout->setContentsMargins(10, 8, 10, 8);
    detailsLayout->setSpacing(3);

    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);

    m_titleLabel = new QLabel(m_detailsPage);
    m_titleLabel->setObjectName(QStringLiteral("targetWindowDetailTitle"));
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_titleLabel->setWordWrap(false);

    m_statusLabel = new QLabel(m_detailsPage);
    m_statusLabel->setObjectName(QStringLiteral("targetWindowDetailStatus"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setMinimumWidth(82);

    m_moreButton = new QToolButton(m_detailsPage);
    m_moreButton->setObjectName(QStringLiteral("targetWindowDetailMoreButton"));
    m_moreButton->setText(tr("더 보기"));
    m_moreButton->setCursor(Qt::PointingHandCursor);
    m_moreButton->setCheckable(true);
    m_moreButton->setAutoRaise(true);
    m_moreButton->setFocusPolicy(Qt::NoFocus);
    m_moreButton->setMinimumHeight(22);
    m_moreButton->setStyleSheet(QStringLiteral(
        "QToolButton#targetWindowDetailMoreButton {"
        "  padding: 2px 8px;"
        "  border: none;"
        "  background: transparent;"
        "}"));
    m_moreButton->setVisible(true);

    titleRow->addWidget(m_titleLabel, 1);
    titleRow->addWidget(m_statusLabel, 0);
    titleRow->addWidget(m_moreButton, 0);

    m_primaryLine = new QLabel(m_detailsPage);
    m_primaryLine->setObjectName(QStringLiteral("targetWindowDetailPrimary"));
    m_primaryLine->setTextFormat(Qt::RichText);
    m_primaryLine->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_primaryLine->setWordWrap(false);

    m_secondaryLine = new QLabel(m_detailsPage);
    m_secondaryLine->setObjectName(QStringLiteral("targetWindowDetailSecondary"));
    m_secondaryLine->setTextFormat(Qt::RichText);
    m_secondaryLine->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_secondaryLine->setWordWrap(false);

    m_tertiaryLine = new QLabel(m_detailsPage);
    m_tertiaryLine->setObjectName(QStringLiteral("targetWindowDetailTertiary"));
    m_tertiaryLine->setTextFormat(Qt::RichText);
    m_tertiaryLine->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_tertiaryLine->setWordWrap(false);

    detailsLayout->addLayout(titleRow);
    detailsLayout->addWidget(m_primaryLine);
    detailsLayout->addWidget(m_secondaryLine);
    detailsLayout->addWidget(m_tertiaryLine);

    m_tertiaryLine->setVisible(m_expandedDetails);

    connect(m_moreButton, &QToolButton::toggled, this, [this](bool expanded) {
        m_expandedDetails = expanded;
        m_moreButton->setText(expanded ? tr("접기") : tr("더 보기"));
        if (m_tertiaryLine) {
            m_tertiaryLine->setVisible(expanded);
        }
        updateThemeColors();
    });

    stackedLayout->addWidget(m_messagePage);
    stackedLayout->addWidget(m_detailsPage);
    outerLayout->addWidget(stackHost);

    setStyleSheet(QStringLiteral(
        "QFrame#targetWindowDetailPanel {"
        "  background-color: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "}"
        "QLabel#targetWindowDetailStatus {"
        "  padding: 2px 8px;"
        "  border-radius: 999px;"
        "  font-weight: 600;"
        "}"));
    // Static stylesheet only — never call setStyleSheet from changeEvent (see AGENTS.md §8.4).

    updateThemeColors();
    showMessage(tr("'창 지정'으로 대상 창을 선택하세요."));
}

void TargetWindowDetailPanel::setLabelTextColor(QLabel* label,
                                                const QColor& color,
                                                int fontSizePx,
                                                bool bold) const {
    if (!label) {
        return;
    }

    QFont font = label->font();
    font.setPointSizeF(-1);
    font.setPixelSize(fontSizePx);
    font.setBold(bold);
    label->setFont(font);

    QPalette labelPalette = label->palette();
    labelPalette.setColor(QPalette::WindowText, color);
    labelPalette.setColor(QPalette::Text, color);
    label->setPalette(labelPalette);
}

QColor TargetWindowDetailPanel::stateColorFor(const TargetWindowDetailData& data) const {
    const QPalette pal = palette();
    const QColor text = pal.color(QPalette::WindowText);
    if (data.minimized) {
        return text.lightness() < 128 ? QColor(0xff, 0xc1, 0x07) : QColor(0xb8, 0x86, 0x0b);
    }
    if (data.visible) {
        return text.lightness() < 128 ? QColor(0x81, 0xc7, 0x84) : QColor(0x2f, 0x9e, 0x62);
    }

    QColor stateColor = pal.color(QPalette::Disabled, QPalette::Text);
    if (!stateColor.isValid() || stateColor == text) {
        stateColor = text.lightness() < 128 ? QColor(0x9a, 0xa0, 0xa6) : QColor(0x75, 0x7b, 0x80);
    }
    return stateColor;
}

void TargetWindowDetailPanel::refreshDetailText() {
    if (m_lastDetailData.hwnd.isEmpty()) {
        return;
    }

    const QPalette pal = palette();
    const QColor text = pal.color(QPalette::WindowText);
    const QColor muted = secondaryHintTextColor(pal);
    const QColor stateColor = stateColorFor(m_lastDetailData);
    const QString sep = QStringLiteral(" &nbsp;&middot;&nbsp; ");
    const QString title = m_lastDetailData.title.isEmpty() ? tr("이름 없는 창") : m_lastDetailData.title;

    m_titleLabel->setText(title);
    m_statusLabel->setText(m_lastDetailData.stateText);

    const QColor badgeText = stateColor.lightness() < 140 ? QColor(Qt::white) : QColor(0x10, 0x18, 0x20);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 3px 10px; border-radius: 999px; font-weight: 600; background-color: %1; color: %2;")
                                     .arg(stateColor.name(), badgeText.name()));

    const QString primary =
        captionSpan(tr("프로세스"), muted) + QLatin1Char(' ')
        + valueSpan(m_lastDetailData.processName, text) + sep + captionSpan(tr("클래스"), muted)
        + QLatin1Char(' ') + valueSpan(m_lastDetailData.className, text) + sep
        + captionSpan(QStringLiteral("HWND"), muted) + QLatin1Char(' ')
        + valueSpan(m_lastDetailData.hwnd, text);

    const QString secondary =
        captionSpan(tr("창 영역"), muted) + QLatin1Char(' ')
        + valueSpan(m_lastDetailData.frameBounds, text) + sep + captionSpan(tr("클라이언트"), muted)
        + QLatin1Char(' ') + valueSpan(m_lastDetailData.clientSize, text);

    const QString tertiary =
        captionSpan(tr("모니터"), muted) + QLatin1Char(' ')
        + valueSpan(m_lastDetailData.monitor, text);

    m_primaryLine->setText(primary);
    m_secondaryLine->setText(secondary);
    m_tertiaryLine->setText(tertiary);

    if (m_tertiaryLine) {
        m_tertiaryLine->setVisible(m_expandedDetails);
    }
}

void TargetWindowDetailPanel::updateThemeColors() {
    if (m_updatingTheme) {
        return;
    }
    m_updatingTheme = true;

    const QPalette pal = palette();
    const QColor text = pal.color(QPalette::WindowText);
    const QColor muted = secondaryHintTextColor(pal);

    setLabelTextColor(m_messageLabel, muted, 12, false);
    setLabelTextColor(m_titleLabel, text, 13, true);
    setLabelTextColor(m_primaryLine, text, 10, false);
    setLabelTextColor(m_secondaryLine, text, 10, false);
    setLabelTextColor(m_tertiaryLine, text, 10, false);

    if (m_moreButton) {
        QPalette btnPal = m_moreButton->palette();
        btnPal.setColor(QPalette::ButtonText, text);
        btnPal.setColor(QPalette::WindowText, text);
        m_moreButton->setPalette(btnPal);
        QFont f = m_moreButton->font();
        f.setPixelSize(12);
        m_moreButton->setFont(f);
    }

    if (m_detailsPage->isVisible() && m_globalDefaultProfileMode) {
        refreshGlobalDefaultProfileText();
    } else if (m_detailsPage->isVisible() && m_storedTargetBindingMode) {
        refreshStoredTargetBindingText(m_storedBindingTitle,
                                       m_storedBindingProcessName,
                                       m_storedBindingProcessPath);
    } else if (m_detailsPage->isVisible() && !m_lastDetailData.hwnd.isEmpty()) {
        refreshDetailText();
    }

    m_updatingTheme = false;
}

void TargetWindowDetailPanel::changeEvent(QEvent* event) {
    QFrame::changeEvent(event);
    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        updateThemeColors();
        break;
    default:
        break;
    }
}

void TargetWindowDetailPanel::showMessage(const QString& message) {
    m_globalDefaultProfileMode = false;
    m_storedTargetBindingMode = false;
    m_messageLabel->setText(message);
    setToolTip({});
    if (auto* stackHost = m_messagePage->parentWidget()) {
        if (auto* stackedLayout = qobject_cast<QStackedLayout*>(stackHost->layout())) {
            stackedLayout->setCurrentWidget(m_messagePage);
        }
    }
    updateThemeColors();
}

void TargetWindowDetailPanel::showDetails(const TargetWindowDetailData& data) {
    m_globalDefaultProfileMode = false;
    m_storedTargetBindingMode = false;
    m_lastDetailData = data;

    if (auto* stackHost = m_messagePage->parentWidget()) {
        if (auto* stackedLayout = qobject_cast<QStackedLayout*>(stackHost->layout())) {
            stackedLayout->setCurrentWidget(m_detailsPage);
        }
    }

    refreshDetailText();
    if (m_moreButton) {
        m_moreButton->setVisible(true);
    }
    updateThemeColors();

    QString panelTooltip = tr("프로세스 경로: %1").arg(data.processPath.isEmpty() ? tr("알 수 없음") : data.processPath);
    panelTooltip += QStringLiteral("\n") + tr("창·모니터 크기는 물리 픽셀 기준입니다.");
    if (data.monitorDpi > 0) {
        panelTooltip += QStringLiteral("\n") + tr("모니터 DPI는 대상 창이 위치한 디스플레이 기준입니다.");
    }
    setToolTip(panelTooltip);
}

void TargetWindowDetailPanel::showGlobalDefaultProfile() {
    m_globalDefaultProfileMode = true;
    m_storedTargetBindingMode = false;
    m_lastDetailData = {};

    if (auto* stackHost = m_messagePage->parentWidget()) {
        if (auto* stackedLayout = qobject_cast<QStackedLayout*>(stackHost->layout())) {
            stackedLayout->setCurrentWidget(m_detailsPage);
        }
    }

    if (m_moreButton) {
        m_moreButton->setVisible(false);
        m_moreButton->setChecked(false);
    }
    if (m_tertiaryLine) {
        m_tertiaryLine->setVisible(false);
    }

    refreshGlobalDefaultProfileText();
    updateThemeColors();

    setToolTip(tr("전역 기본 프로필은 대상 창을 지정하지 않습니다.\n"
                  "연결된 프로그램이 없을 때 자동으로 선택되며, 대상 창 설정은 변경할 수 없습니다."));
}

void TargetWindowDetailPanel::refreshGlobalDefaultProfileText() {
    const QPalette pal = palette();
    const QColor text = pal.color(QPalette::WindowText);
    const QColor muted = secondaryHintTextColor(pal);
    const QColor stateColor = text.lightness() < 128 ? QColor(0x64, 0xb5, 0xf6) : QColor(0x1e, 0x88, 0xe5);

    m_titleLabel->setText(tr("전역 기본 프로필"));
    m_statusLabel->setText(tr("● 시스템 고정"));

    const QColor badgeText = stateColor.lightness() < 140 ? QColor(Qt::white) : QColor(0x10, 0x18, 0x20);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 3px 10px; border-radius: 999px; font-weight: 600; background-color: %1; color: %2;")
                                     .arg(stateColor.name(), badgeText.name()));

    m_primaryLine->setText(
        captionSpan(tr("대상 창"), muted) + QLatin1Char(' ')
        + valueSpan(tr("미지정"), text) + QStringLiteral(" &nbsp;&middot;&nbsp; ")
        + captionSpan(tr("실행 범위"), muted) + QLatin1Char(' ')
        + valueSpan(tr("전역"), text));

    m_secondaryLine->setText(valueSpan(tr("모든 프로그램에서 동작하며, 대상 창 설정은 변경할 수 없습니다."), muted));
}

void TargetWindowDetailPanel::showStoredTargetBinding(const QString& title,
                                                      const QString& processName,
                                                      const QString& processPath) {
    m_globalDefaultProfileMode = false;
    m_storedTargetBindingMode = true;
    m_lastDetailData = {};
    m_storedBindingTitle = title;
    m_storedBindingProcessName = processName;
    m_storedBindingProcessPath = processPath;

    if (auto* stackHost = m_messagePage->parentWidget()) {
        if (auto* stackedLayout = qobject_cast<QStackedLayout*>(stackHost->layout())) {
            stackedLayout->setCurrentWidget(m_detailsPage);
        }
    }

    if (m_moreButton) {
        m_moreButton->setVisible(false);
        m_moreButton->setChecked(false);
    }
    if (m_tertiaryLine) {
        m_tertiaryLine->setVisible(false);
    }

    refreshStoredTargetBindingText(title, processName, processPath);
    updateThemeColors();

    QString panelTooltip = tr("저장된 연결 프로그램입니다. 현재 실행 중인 창이 감지되지 않습니다.");
    if (!processPath.isEmpty()) {
        panelTooltip += QStringLiteral("\n") + tr("프로세스 경로: %1").arg(processPath);
    }
    setToolTip(panelTooltip);
}

void TargetWindowDetailPanel::refreshStoredTargetBindingText(const QString& title,
                                                             const QString& processName,
                                                             const QString& processPath) {
    const QPalette pal = palette();
    const QColor text = pal.color(QPalette::WindowText);
    const QColor muted = secondaryHintTextColor(pal);
    const QColor stateColor = text.lightness() < 128 ? QColor(0xff, 0xc1, 0x07) : QColor(0xb8, 0x86, 0x0b);

    const QString displayTitle = title.isEmpty() ? tr("(제목 없음)") : title;
    const QString displayProcess =
        processName.isEmpty() ? tr("알 수 없음") : processName;

    m_titleLabel->setText(displayTitle);
    m_statusLabel->setText(tr("● 미실행"));

    const QColor badgeText = stateColor.lightness() < 140 ? QColor(Qt::white) : QColor(0x10, 0x18, 0x20);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 3px 10px; border-radius: 999px; font-weight: 600; background-color: %1; color: %2;")
                                     .arg(stateColor.name(), badgeText.name()));

    m_primaryLine->setText(
        captionSpan(tr("프로세스"), muted) + QLatin1Char(' ')
        + valueSpan(displayProcess, text) + QStringLiteral(" &nbsp;&middot;&nbsp; ")
        + captionSpan(tr("연결 제목"), muted) + QLatin1Char(' ')
        + valueSpan(displayTitle, text));

    m_secondaryLine->setText(
        valueSpan(tr("현재 실행 중인 창이 감지되지 않습니다. 프로그램을 실행하면 자동으로 연결됩니다."),
                  muted));
    Q_UNUSED(processPath);
}
