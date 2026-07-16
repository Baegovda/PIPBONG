#include "ui/widgets/WorkflowRunStatusBar.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>

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
    row->setContentsMargins(10, 6, 10, 6);
    row->setSpacing(8);

    m_promptLabel = new QLabel(QStringLiteral("▸"), this);
    m_promptLabel->setObjectName(QStringLiteral("workflowRunStatusPrompt"));

    m_captionLabel = new QLabel(tr("워크플로우"), this);
    m_captionLabel->setObjectName(QStringLiteral("workflowRunStatusCaption"));

    m_featureNameLabel = new QLabel(tr("—"), this);
    m_featureNameLabel->setObjectName(QStringLiteral("workflowRunStatusFeatureName"));
    m_featureNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

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

    row->addWidget(m_promptLabel, 0, Qt::AlignVCenter);
    row->addWidget(m_captionLabel, 0, Qt::AlignVCenter);
    row->addWidget(m_featureNameLabel, 0, Qt::AlignVCenter);
    row->addStretch(1);
    row->addWidget(m_statsRow, 0, Qt::AlignVCenter);

    applyTerminalChrome();
    clearLoopTiming();
    setFeatureName(QString());
}

void WorkflowRunStatusBar::applyTerminalChrome() {
    setStyleSheet(QStringLiteral(
        "QFrame#workflowRunStatusBar {"
        "  background-color: #161b22;"
        "  border: 1px solid #30363d;"
        "  border-radius: 8px;"
        "}"
        "QLabel#workflowRunStatusPrompt {"
        "  color: #58a6ff;"
        "  font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
        "  font-size: 13px;"
        "  font-weight: 700;"
        "  padding-right: 2px;"
        "}"
        "QLabel#workflowRunStatusCaption {"
        "  color: #8b949e;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  letter-spacing: 0.4px;"
        "}"
        "QLabel#workflowRunStatusFeatureName {"
        "  color: #e6edf3;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  padding-left: 2px;"
        "}"
        "QLabel#workflowRunStatLoop {"
        "  color: #79c0ff;"
        "  background-color: #0d1117;"
        "  border: 1px solid #388bfd;"
        "  border-radius: 6px;"
        "  padding: 3px 8px;"
        "  font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "QLabel#workflowRunStatLast {"
        "  color: #e6edf3;"
        "  background-color: #21262d;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 3px 8px;"
        "  font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "QLabel#workflowRunStatAverage {"
        "  color: #c9d1d9;"
        "  background-color: #21262d;"
        "  border: 1px solid #30363d;"
        "  border-radius: 6px;"
        "  padding: 3px 8px;"
        "  font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
        "  font-size: 11px;"
        "}"
        ));
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
        m_featureNameLabel->setToolTip(QString());
        return;
    }
    m_featureNameLabel->setText(trimmed);
    m_featureNameLabel->setToolTip(trimmed);
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
            "background-color: #0d1117;"
            "border: 1px solid #238636;"
            "border-radius: 6px;"
            "padding: 3px 10px;"
            "font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
            "font-size: 11px;"
            "font-weight: 700;"
            "letter-spacing: 0.3px;");
        static const QString kFailStyle = QStringLiteral(
            "color: #f85149;"
            "background-color: #0d1117;"
            "border: 1px solid #da3633;"
            "border-radius: 6px;"
            "padding: 3px 10px;"
            "font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
            "font-size: 11px;"
            "font-weight: 700;"
            "letter-spacing: 0.3px;");
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
