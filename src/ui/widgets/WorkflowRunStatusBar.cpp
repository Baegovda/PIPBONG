#include "ui/widgets/WorkflowRunStatusBar.h"

#include "ui/FeatureRunModeTheme.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QToolButton>

namespace {

QString formatLoopMs(qint64 ms) {
    return QLocale().toString(ms) + QStringLiteral(" ms");
}

QLabel* makeStatChip(QWidget* parent, const QString& objectName) {
    auto* chip = new QLabel(parent);
    chip->setObjectName(objectName);
    chip->setAlignment(Qt::AlignCenter);
    chip->setTextFormat(Qt::PlainText);
    chip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    return chip;
}

} // namespace

WorkflowRunStatusBar::WorkflowRunStatusBar(QWidget* parent)
    : QFrame(parent) {
    setObjectName(QStringLiteral("workflowRunStatusBar"));
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(6, 4, 6, 4);
    row->setSpacing(6);

    m_runButton = new QToolButton(this);
    m_runButton->setObjectName(QStringLiteral("workflowRunStatusRunButton"));
    m_runButton->setText(QStringLiteral("▶"));
    m_runButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_runButton->setAutoRaise(false);
    m_runButton->setCursor(Qt::PointingHandCursor);
    m_runButton->setFixedSize(22, 22);
    m_runButton->setToolTip(tr("기능 실행 / 중지"));
    connect(m_runButton, &QToolButton::clicked, this, &WorkflowRunStatusBar::runToggleRequested);

    m_profileLabel = new QLabel(tr("—"), this);
    m_profileLabel->setObjectName(QStringLiteral("workflowRunStatusProfile"));

    m_breadcrumbSep1 = new QLabel(QStringLiteral("›"), this);
    m_breadcrumbSep1->setObjectName(QStringLiteral("workflowRunStatusBreadcrumbSep"));

    m_featureNameLabel = new QLabel(tr("—"), this);
    m_featureNameLabel->setObjectName(QStringLiteral("workflowRunStatusFeatureName"));
    m_featureNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_breadcrumbSep2 = new QLabel(QStringLiteral("›"), this);
    m_breadcrumbSep2->setObjectName(QStringLiteral("workflowRunStatusBreadcrumbSep"));

    m_modeChip = makeStatChip(this, QStringLiteral("workflowRunStatusMode"));
    m_modeChip->setVisible(false);

    m_statsRow = new QWidget(this);
    m_statsRow->setObjectName(QStringLiteral("workflowRunStatsRow"));
    auto* statsLayout = new QHBoxLayout(m_statsRow);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(6);

    m_loopChip = makeStatChip(m_statsRow, QStringLiteral("workflowRunStatLoop"));
    m_lastChip = makeStatChip(m_statsRow, QStringLiteral("workflowRunStatLast"));
    m_averageChip = makeStatChip(m_statsRow, QStringLiteral("workflowRunStatAverage"));
    m_statusChip = makeStatChip(m_statsRow, QStringLiteral("workflowRunStatStatus"));

    statsLayout->addWidget(m_loopChip);
    statsLayout->addWidget(m_lastChip);
    statsLayout->addWidget(m_averageChip);
    statsLayout->addWidget(m_statusChip);

    row->addWidget(m_runButton, 0, Qt::AlignVCenter);
    row->addWidget(m_profileLabel, 0, Qt::AlignVCenter);
    row->addWidget(m_breadcrumbSep1, 0, Qt::AlignVCenter);
    row->addWidget(m_featureNameLabel, 0, Qt::AlignVCenter);
    row->addWidget(m_breadcrumbSep2, 0, Qt::AlignVCenter);
    row->addWidget(m_modeChip, 0, Qt::AlignVCenter);
    row->addStretch(1);
    row->addWidget(m_statsRow, 0, Qt::AlignVCenter);

    applyTerminalChrome();
    refreshModeChipChrome();
    setRunButtonState(false, false);
    setProfileName(QString());
    clearRunMode();
    clearLoopTiming();
    setFeatureName(QString());
}

QString WorkflowRunStatusBar::runModeText(FeatureRunMode mode, int repeatCount) const {
    switch (mode) {
    case FeatureRunMode::Hold:
        return tr("홀드");
    case FeatureRunMode::RepeatInfinite:
        return tr("무한 반복");
    case FeatureRunMode::Trigger:
        return tr("트리거");
    case FeatureRunMode::RepeatCount: {
        const int count = repeatCount < 1 ? 1 : repeatCount;
        return tr("%1회 반복").arg(count);
    }
    }
    return QString();
}

QString WorkflowRunStatusBar::runModeToolTip(FeatureRunMode mode, int repeatCount) const {
    switch (mode) {
    case FeatureRunMode::Hold:
        return tr("단축키를 누르고 있는 동안 워크플로를 반복합니다.");
    case FeatureRunMode::RepeatInfinite:
        return tr("워크플로를 중지할 때까지 무한 반복합니다.");
    case FeatureRunMode::Trigger:
        return tr("첫 템플릿 매칭을 감시하다가 성공 시 워크플로를 1회 실행합니다.");
    case FeatureRunMode::RepeatCount: {
        const int count = repeatCount < 1 ? 1 : repeatCount;
        return tr("워크플로를 %1회 반복합니다.").arg(count);
    }
    }
    return QString();
}

void WorkflowRunStatusBar::setProfileName(const QString& name) {
    m_profileName = name.trimmed();
    refreshBreadcrumb();
}

void WorkflowRunStatusBar::refreshBreadcrumb() {
    const QString emptyDash = tr("—");
    const QString profileText = m_profileName.isEmpty() ? emptyDash : m_profileName;
    const QString featureText =
        m_featureNameLabel ? m_featureNameLabel->text().trimmed() : emptyDash;
    const bool hasFeature = !featureText.isEmpty() && featureText != emptyDash;
    const bool hasMode = m_hasRunMode && m_modeChip && !m_modeChip->text().isEmpty();

    if (m_profileLabel) {
        m_profileLabel->setText(profileText);
        m_profileLabel->setToolTip(m_profileName.isEmpty() ? tr("프로필") : tr("프로필: %1").arg(m_profileName));
    }
    if (m_featureNameLabel && hasFeature) {
        m_featureNameLabel->setToolTip(tr("기능: %1").arg(featureText));
    } else if (m_featureNameLabel) {
        m_featureNameLabel->setToolTip(tr("기능"));
    }
    if (m_breadcrumbSep1) {
        m_breadcrumbSep1->setVisible(true);
    }
    if (m_breadcrumbSep2) {
        m_breadcrumbSep2->setVisible(hasFeature && hasMode);
    }
    if (m_modeChip) {
        m_modeChip->setVisible(hasMode);
    }
}

void WorkflowRunStatusBar::setRunMode(FeatureRunMode mode, int repeatCount) {
    if (!m_modeChip) {
        return;
    }
    m_runMode = mode;
    m_repeatCount = repeatCount;
    const QString text = runModeText(mode, repeatCount);
    m_hasRunMode = !text.isEmpty();
    m_modeChip->setText(text);
    m_modeChip->setToolTip(runModeToolTip(mode, repeatCount));
    refreshModeChipChrome();
    refreshBreadcrumb();
}

void WorkflowRunStatusBar::refreshModeChipChrome() {
    if (!m_modeChip) {
        return;
    }
    if (!m_hasRunMode || m_modeChip->text().isEmpty()) {
        m_modeChip->setStyleSheet(QString());
        return;
    }
    m_modeChip->setStyleSheet(featureRunModeChipStyleSheet(
        m_runMode, true, QStringLiteral("QLabel#workflowRunStatusMode")));
}

void WorkflowRunStatusBar::clearRunMode() {
    if (!m_modeChip) {
        return;
    }
    m_hasRunMode = false;
    m_modeChip->clear();
    m_modeChip->setToolTip(QString());
    refreshBreadcrumb();
}

void WorkflowRunStatusBar::setRunButtonState(bool showStop, bool enabled, const QString& disabledToolTip) {
    m_runButtonShowStop = showStop;
    m_runButtonEnabled = enabled;
    if (!m_runButton) {
        return;
    }
    m_runButton->setText(showStop ? QStringLiteral("■") : QStringLiteral("▶"));
    m_runButton->setEnabled(enabled);
    if (enabled) {
        m_runButton->setToolTip(showStop ? tr("실행 중지") : tr("기능 실행"));
    } else if (!disabledToolTip.isEmpty()) {
        m_runButton->setToolTip(disabledToolTip);
    } else {
        m_runButton->setToolTip(tr("기능 실행 / 중지"));
    }
    refreshRunButtonChrome();
}

void WorkflowRunStatusBar::refreshRunButtonChrome() {
    if (!m_runButton) {
        return;
    }
    if (!m_runButtonEnabled) {
        m_runButton->setStyleSheet(QStringLiteral(
            "QToolButton#workflowRunStatusRunButton {"
            "  color: #8b949e;"
            "  background-color: transparent;"
            "  border: none;"
            "  border-radius: 3px;"
            "  font-size: 11px;"
            "  font-weight: 700;"
            "  padding: 0;"
            "}"));
        return;
    }
    if (m_runButtonShowStop) {
        m_runButton->setStyleSheet(QStringLiteral(
            "QToolButton#workflowRunStatusRunButton {"
            "  color: #3fb950;"
            "  background-color: transparent;"
            "  border: none;"
            "  border-radius: 3px;"
            "  font-size: 10px;"
            "  font-weight: 700;"
            "  padding: 0;"
            "}"
            "QToolButton#workflowRunStatusRunButton:hover {"
            "  background-color: rgba(46, 160, 67, 0.15);"
            "}"
            "QToolButton#workflowRunStatusRunButton:pressed {"
            "  background-color: rgba(46, 160, 67, 0.25);"
            "}"));
        return;
    }
    m_runButton->setStyleSheet(QStringLiteral(
        "QToolButton#workflowRunStatusRunButton {"
        "  color: #58a6ff;"
        "  background-color: transparent;"
        "  border: none;"
        "  border-radius: 3px;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "  padding: 0;"
        "  padding-left: 1px;"
        "}"
        "QToolButton#workflowRunStatusRunButton:hover {"
        "  background-color: rgba(56, 139, 253, 0.15);"
        "}"
        "QToolButton#workflowRunStatusRunButton:pressed {"
        "  background-color: rgba(56, 139, 253, 0.25);"
        "}"));
}

void WorkflowRunStatusBar::applyTerminalChrome() {
    setStyleSheet(QStringLiteral(
        "QFrame#workflowRunStatusBar {"
        "  background-color: #161b22;"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "QLabel#workflowRunStatusProfile {"
        "  color: #8b949e;"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 0 2px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "QLabel#workflowRunStatusBreadcrumbSep {"
        "  color: #484f58;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  padding: 0 1px;"
        "}"
        "QLabel#workflowRunStatusFeatureName {"
        "  color: #e6edf3;"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 0 2px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "QLabel#workflowRunStatLoop {"
        "  color: #79c0ff;"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 0 4px;"
        "  font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "QLabel#workflowRunStatLast {"
        "  color: #e6edf3;"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 0 4px;"
        "  font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "QLabel#workflowRunStatAverage {"
        "  color: #c9d1d9;"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 0 4px;"
        "  font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
        "  font-size: 11px;"
        "}"
        ));
    refreshRunButtonChrome();
}

void WorkflowRunStatusBar::refreshStatsVisibility() {
    if (m_statsRow) {
        m_statsRow->setVisible(m_hasLoopTiming);
    }
}

void WorkflowRunStatusBar::setFeatureName(const QString& name) {
    if (!m_featureNameLabel) {
        return;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        m_featureNameLabel->setText(tr("—"));
    } else {
        m_featureNameLabel->setText(trimmed);
    }
    refreshBreadcrumb();
}

void WorkflowRunStatusBar::setLoopTiming(int loopNumber, qint64 elapsedMs, qint64 averageMs, bool success) {
    m_hasLoopTiming = true;
    if (m_loopChip) {
        m_loopChip->setText(tr("루프 %1").arg(loopNumber));
        m_loopChip->setToolTip(tr("마지막으로 완료한 루프 번호"));
    }
    if (m_lastChip) {
        m_lastChip->setText(tr("직전 %1").arg(formatLoopMs(elapsedMs)));
        m_lastChip->setToolTip(tr("마지막 루프 소요 시간"));
    }
    if (m_averageChip) {
        m_averageChip->setText(tr("평균 %1").arg(formatLoopMs(averageMs)));
        m_averageChip->setToolTip(tr("이번 실행 세션의 루프 평균 소요 시간"));
    }
    if (m_statusChip) {
        static const QString kSuccessStyle = QStringLiteral(
            "color: #3fb950;"
            "background-color: transparent;"
            "border: none;"
            "padding: 0 4px;"
            "font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
            "font-size: 11px;"
            "font-weight: 700;");
        static const QString kFailStyle = QStringLiteral(
            "color: #f85149;"
            "background-color: transparent;"
            "border: none;"
            "padding: 0 4px;"
            "font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
            "font-size: 11px;"
            "font-weight: 700;");
        if (success) {
            m_statusChip->setStyleSheet(kSuccessStyle);
            m_statusChip->setText(tr("성공"));
            m_statusChip->setToolTip(tr("마지막 루프가 성공적으로 끝났습니다"));
        } else {
            m_statusChip->setStyleSheet(kFailStyle);
            m_statusChip->setText(tr("실패"));
            m_statusChip->setToolTip(tr("마지막 루프가 실패했습니다"));
        }
    }
    refreshStatsVisibility();
}

void WorkflowRunStatusBar::clearLoopTiming() {
    m_hasLoopTiming = false;
    refreshStatsVisibility();
}
