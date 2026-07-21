#include "ui/TargetWindowDetailPanel.h"

#include "ui/UiThemeColors.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace {

constexpr int kComfortableDetailWidthPx = 440;
constexpr int kCompactDetailWidthPx = 280;
constexpr int kDetailHorizontalMarginPx = 20;

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
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setMinimumHeight(48);
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

    m_titleRowWidget = new QWidget(m_detailsPage);
    m_titleLabel = new QLabel(m_titleRowWidget);
    m_titleLabel->setObjectName(QStringLiteral("targetWindowDetailTitle"));
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_titleLabel->setWordWrap(true);

    m_statusLabel = new QLabel(m_titleRowWidget);
    m_statusLabel->setObjectName(QStringLiteral("targetWindowDetailStatus"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    m_primaryLine = new QLabel(m_detailsPage);
    m_primaryLine->setObjectName(QStringLiteral("targetWindowDetailPrimary"));
    m_primaryLine->setTextFormat(Qt::RichText);
    m_primaryLine->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_primaryLine->setWordWrap(true);

    m_secondaryLine = new QLabel(m_detailsPage);
    m_secondaryLine->setObjectName(QStringLiteral("targetWindowDetailSecondary"));
    m_secondaryLine->setTextFormat(Qt::RichText);
    m_secondaryLine->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_secondaryLine->setWordWrap(true);

    detailsLayout->addWidget(m_titleRowWidget);
    detailsLayout->addWidget(m_primaryLine);
    detailsLayout->addWidget(m_secondaryLine);

    rebuildTitleRowLayout();

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
    showMessage(tr("'타겟 지정'으로 타겟을 선택하세요."));
    updateAdaptiveLayout();
}

void TargetWindowDetailPanel::rebuildTitleRowLayout() {
    if (!m_titleRowWidget || !m_titleLabel || !m_statusLabel) {
        return;
    }

    delete m_titleRowWidget->layout();
    m_titleRowLayout = nullptr;

    const bool horizontalTitle = m_layoutDensity == DetailLayoutDensity::Comfortable;
    if (horizontalTitle) {
        auto* row = new QHBoxLayout(m_titleRowWidget);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(6);
        row->addWidget(m_titleLabel, 1);
        row->addWidget(m_statusLabel, 0, Qt::AlignVCenter);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_titleRowLayout = row;
        return;
    }

    auto* column = new QVBoxLayout(m_titleRowWidget);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(2);
    column->addWidget(m_titleLabel);
    column->addWidget(m_statusLabel, 0, Qt::AlignLeft);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_titleRowLayout = column;
}

void TargetWindowDetailPanel::updateAdaptiveLayout() {
    const int contentWidth = qMax(0, width() - kDetailHorizontalMarginPx);
    const DetailLayoutDensity newDensity =
        densityForWidth(contentWidth);
    const bool titleLayoutChanged =
        !m_layoutReady || newDensity == DetailLayoutDensity::Comfortable
        || m_layoutDensity == DetailLayoutDensity::Comfortable;
    if (newDensity == m_layoutDensity && m_layoutReady && !titleLayoutChanged) {
        return;
    }

    m_layoutDensity = newDensity;
    m_layoutReady = true;
    if (titleLayoutChanged) {
        rebuildTitleRowLayout();
    }
    refreshVisibleDetailText();
}

void TargetWindowDetailPanel::refreshVisibleDetailText() {
    if (m_globalDefaultProfileMode) {
        refreshGlobalDefaultProfileText();
    } else if (m_storedTargetBindingMode) {
        refreshStoredTargetBindingText(m_storedBindingTitle,
                                       m_storedBindingProcessName,
                                       m_storedBindingProcessPath);
    } else if (!m_lastDetailData.hwnd.isEmpty()) {
        refreshDetailText();
    }
}

void TargetWindowDetailPanel::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    updateAdaptiveLayout();
}

TargetWindowDetailPanel::DetailLayoutDensity TargetWindowDetailPanel::densityForWidth(
    int contentWidthPx) const {
    if (contentWidthPx >= kComfortableDetailWidthPx) {
        return DetailLayoutDensity::Comfortable;
    }
    if (contentWidthPx >= kCompactDetailWidthPx) {
        return DetailLayoutDensity::Compact;
    }
    return DetailLayoutDensity::Narrow;
}

QString TargetWindowDetailPanel::formatFieldsHtml(const QVector<QPair<QString, QString>>& fields,
                                                  DetailLayoutDensity density,
                                                  const QColor& muted,
                                                  const QColor& text) const {
    if (fields.isEmpty()) {
        return {};
    }

    const auto joinField = [&](const QPair<QString, QString>& field) {
        return captionSpan(field.first, muted) + QLatin1Char(' ') + valueSpan(field.second, text);
    };

    const QString sep = QStringLiteral(" &nbsp;&middot;&nbsp; ");
    if (density == DetailLayoutDensity::Comfortable) {
        QStringList parts;
        parts.reserve(fields.size());
        for (const auto& field : fields) {
            parts.push_back(joinField(field));
        }
        return parts.join(sep);
    }

    if (density == DetailLayoutDensity::Compact) {
        const int splitAt = (fields.size() + 1) / 2;
        QString line1;
        QString line2;
        for (int i = 0; i < fields.size(); ++i) {
            const QString chunk = joinField(fields[i]);
            if (i < splitAt) {
                if (!line1.isEmpty()) {
                    line1 += sep;
                }
                line1 += chunk;
            } else {
                if (!line2.isEmpty()) {
                    line2 += sep;
                }
                line2 += chunk;
            }
        }
        if (line2.isEmpty()) {
            return line1;
        }
        return line1 + QStringLiteral("<br>") + line2;
    }

    QStringList lines;
    lines.reserve(fields.size());
    for (const auto& field : fields) {
        lines.push_back(joinField(field));
    }
    return lines.join(QStringLiteral("<br>"));
}

void TargetWindowDetailPanel::setLabelTextColor(QLabel* label,
                                                const QColor& color,
                                                int fontSizePx,
                                                bool bold) const {
    if (!label) {
        return;
    }

    QFont font = label->font();
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
    const QColor stateColor = m_lastDetailData.subTarget
                                  ? (text.lightness() < 128 ? QColor(0x64, 0xb5, 0xf6)
                                                            : QColor(0x1e, 0x88, 0xe5))
                                  : stateColorFor(m_lastDetailData);
    const QString title = m_lastDetailData.title.isEmpty() ? tr("이름 없는 창") : m_lastDetailData.title;

    m_titleLabel->setText(title);
    m_statusLabel->setText(m_lastDetailData.subTarget ? tr("● 서브 창") : m_lastDetailData.stateText);

    const QColor badgeText = stateColor.lightness() < 140 ? QColor(Qt::white) : QColor(0x10, 0x18, 0x20);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 3px 10px; border-radius: 999px; font-weight: 600; background-color: %1; color: %2;")
                                     .arg(stateColor.name(), badgeText.name()));

    const QVector<QPair<QString, QString>> primaryFields = {
        {tr("프로세스"), m_lastDetailData.processName},
        {tr("클래스"), m_lastDetailData.className},
        {QStringLiteral("HWND"), m_lastDetailData.hwnd},
    };
    const QVector<QPair<QString, QString>> secondaryFields = {
        {tr("창 영역"), m_lastDetailData.frameBounds},
        {tr("클라이언트"), m_lastDetailData.clientSize},
        {tr("모니터"), m_lastDetailData.monitor},
    };

    m_primaryLine->setText(formatFieldsHtml(primaryFields, m_layoutDensity, muted, text));
    m_secondaryLine->setText(formatFieldsHtml(secondaryFields, m_layoutDensity, muted, text));
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
    updateThemeColors();

    QString panelTooltip = tr("프로세스 경로: %1").arg(data.processPath.isEmpty() ? tr("알 수 없음") : data.processPath);
    panelTooltip += QStringLiteral("\n") + tr("창·모니터 크기는 물리 픽셀 기준입니다.");
    if (data.monitorDpi > 0) {
        panelTooltip += QStringLiteral("\n") + tr("모니터 DPI는 타겟이 위치한 디스플레이 기준입니다.");
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

    refreshGlobalDefaultProfileText();
    updateThemeColors();

    setToolTip(tr("전역 기본 프로필은 타겟을 지정하지 않습니다.\n"
                  "연결된 프로그램이 없을 때 자동으로 선택되며, 타겟 설정은 변경할 수 없습니다."));
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

    const QVector<QPair<QString, QString>> primaryFields = {
        {tr("타겟"), tr("미지정")},
        {tr("실행 범위"), tr("전역")},
    };
    m_primaryLine->setText(formatFieldsHtml(primaryFields, m_layoutDensity, muted, text));

    m_secondaryLine->setText(valueSpan(tr("모든 프로그램에서 동작하며, 타겟 설정은 변경할 수 없습니다."), muted));
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

    const QVector<QPair<QString, QString>> primaryFields = {
        {tr("프로세스"), displayProcess},
        {tr("연결 제목"), displayTitle},
    };
    m_primaryLine->setText(formatFieldsHtml(primaryFields, m_layoutDensity, muted, text));

    m_secondaryLine->setText(
        valueSpan(tr("현재 실행 중인 창이 감지되지 않습니다. 프로그램을 실행하면 자동으로 연결됩니다."),
                  muted));
    Q_UNUSED(processPath);
}
