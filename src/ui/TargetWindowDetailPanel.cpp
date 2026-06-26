#include "ui/TargetWindowDetailPanel.h"

#include "ui/UiThemeColors.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
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
    detailsLayout->setContentsMargins(10, 7, 10, 7);
    detailsLayout->setSpacing(4);

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

    detailsLayout->addWidget(m_primaryLine);
    detailsLayout->addWidget(m_secondaryLine);

    stackedLayout->addWidget(m_messagePage);
    stackedLayout->addWidget(m_detailsPage);
    outerLayout->addWidget(stackHost);

    setStyleSheet(QStringLiteral(
        "QFrame#targetWindowDetailPanel {"
        "  background-color: palette(window);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
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

    const QString stateLine = QStringLiteral("<span style=\"color:%1;\">%2</span>")
                                  .arg(stateColor.name(), htmlEscape(m_lastDetailData.stateText));

    const QString primary = captionSpan(QStringLiteral("HWND"), muted) + QLatin1Char(' ')
                            + valueSpan(m_lastDetailData.hwnd, text) + sep + stateLine + sep
                            + captionSpan(tr("프로세스"), muted) + QLatin1Char(' ')
                            + valueSpan(m_lastDetailData.processName, text) + sep
                            + captionSpan(tr("제목"), muted) + QLatin1Char(' ')
                            + valueSpan(m_lastDetailData.title, text);

    const QString secondary =
        captionSpan(tr("클래스"), muted) + QLatin1Char(' ')
        + valueSpan(m_lastDetailData.className, text) + sep + captionSpan(QStringLiteral("DWM"), muted)
        + QLatin1Char(' ') + valueSpan(m_lastDetailData.frameBounds, text) + sep
        + captionSpan(tr("클라"), muted) + QLatin1Char(' ')
        + valueSpan(m_lastDetailData.clientSize, text) + sep + captionSpan(tr("모니터"), muted)
        + QLatin1Char(' ') + valueSpan(m_lastDetailData.monitor, text);

    m_primaryLine->setText(primary);
    m_secondaryLine->setText(secondary);
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
    setLabelTextColor(m_primaryLine, text, 11, false);
    setLabelTextColor(m_secondaryLine, text, 11, false);

    if (m_detailsPage->isVisible() && !m_lastDetailData.hwnd.isEmpty()) {
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
    m_lastDetailData = data;

    if (auto* stackHost = m_messagePage->parentWidget()) {
        if (auto* stackedLayout = qobject_cast<QStackedLayout*>(stackHost->layout())) {
            stackedLayout->setCurrentWidget(m_detailsPage);
        }
    }

    refreshDetailText();
    updateThemeColors();

    QString panelTooltip = tr("프로세스 경로: %1").arg(data.processPath.isEmpty() ? tr("알 수 없음") : data.processPath);
    panelTooltip += QStringLiteral("\n") + tr("창·모니터 크기는 물리 픽셀 기준입니다.");
    if (data.monitorDpi > 0) {
        panelTooltip += QStringLiteral("\n") + tr("모니터 DPI는 대상 창이 위치한 디스플레이 기준입니다.");
    }
    setToolTip(panelTooltip);
}
