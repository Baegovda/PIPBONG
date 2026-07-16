#include "ui/diagnostics/SpikeWatchDialog.h"

#include "core/diagnostics/CpuMonitorWorker.h"
#include "ui/UiResizeHandle.h"
#include "ui/widgets/DragAdjustDoubleSpinBox.h"
#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/widgets/HintLabel.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMetaType>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr auto kGeometrySettingsKey = "spikewatch/geometry";
constexpr auto kSectionSplitterSettingsKey = "spikewatch/sectionSplitter";
constexpr auto kIntervalSettingsKey = "spikewatch/intervalMs";
constexpr auto kSystemThresholdSettingsKey = "spikewatch/systemThreshold";
constexpr auto kProcessThresholdSettingsKey = "spikewatch/processThreshold";
constexpr auto kTopNSettingsKey = "spikewatch/topN";
constexpr auto kDeltaMarginSettingsKey = "spikewatch/deltaMargin";
constexpr auto kCooldownSettingsKey = "spikewatch/cooldownMs";

constexpr int kDefaultIntervalMs = 500;
constexpr int kDefaultTopN = 10;
constexpr int kDefaultCooldownMs = 2000;

QString htmlEscape(const QString& text) {
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return escaped;
}

} // namespace

SpikeWatchDialog::SpikeWatchDialog(QWidget* parent)
    : QDialog(parent) {
    setProperty("pipbong_featureHotkeyGateExempt", true);
    qRegisterMetaType<CpuSampleSnapshot>("CpuSampleSnapshot");
    qRegisterMetaType<CpuSpikeEvent>("CpuSpikeEvent");
    qRegisterMetaType<CpuSpikeDetectorConfig>("CpuSpikeDetectorConfig");

    setWindowTitle(tr("CPU 스파이크 감시"));
    setMinimumSize(720, 620);
    setupUi();
    loadPersistedState();

    m_worker = new CpuMonitorWorker;
    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);
    connect(m_worker, &CpuMonitorWorker::sampleReady, this, &SpikeWatchDialog::onSampleReady);
    connect(m_worker, &CpuMonitorWorker::spikeDetected, this, &SpikeWatchDialog::onSpikeDetected);
    connect(m_worker, &CpuMonitorWorker::monitoringStopped, this, &SpikeWatchDialog::onMonitoringStopped);
    m_workerThread->start();
}

SpikeWatchDialog::~SpikeWatchDialog() {
    stopMonitoringAndWait();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(2000);
    }
    delete m_worker;
    m_worker = nullptr;
    persistState();
}

void SpikeWatchDialog::setFeatureRunningCallback(std::function<bool()> callback) {
    m_featureRunningCallback = std::move(callback);
    if (m_worker) {
        m_worker->setFeatureRunningCallback(m_featureRunningCallback);
    }
}

void SpikeWatchDialog::startMonitoringIfIdle() {
    onStartClicked();
}

void SpikeWatchDialog::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto* toolbar = new QHBoxLayout;
    m_startButton = new QPushButton(tr("감시 시작"), this);
    m_stopButton = new QPushButton(tr("중지"), this);
    m_clearLogButton = new QPushButton(tr("로그 지우기"), this);
    m_copyLogButton = new QPushButton(tr("클립보드 복사"), this);
    m_stopButton->setEnabled(false);
    toolbar->addWidget(m_startButton);
    toolbar->addWidget(m_stopButton);
    toolbar->addStretch();
    toolbar->addWidget(m_clearLogButton);
    toolbar->addWidget(m_copyLogButton);
    root->addLayout(toolbar);

    auto* settingsRow = new QHBoxLayout;
    settingsRow->setSpacing(12);

    auto addSetting = [&](const QString& labelText, QWidget* widget) {
        auto* column = new QVBoxLayout;
        column->setSpacing(2);
        auto* label = new QLabel(labelText, this);
        column->addWidget(label);
        column->addWidget(widget);
        settingsRow->addLayout(column);
    };

    m_intervalSpin = new DragAdjustSpinBox(this);
    m_intervalSpin->setRange(200, 2000);
    m_intervalSpin->setSingleStep(50);
    m_intervalSpin->setSuffix(QStringLiteral(" ms"));
    addSetting(tr("샘플 간격"), m_intervalSpin);

    m_systemThresholdSpin = new DragAdjustDoubleSpinBox(this);
    m_systemThresholdSpin->setRange(5.0, 100.0);
    m_systemThresholdSpin->setSingleStep(1.0);
    m_systemThresholdSpin->setDecimals(0);
    m_systemThresholdSpin->setSuffix(QStringLiteral(" %"));
    addSetting(tr("시스템 CPU 임계값"), m_systemThresholdSpin);

    m_processThresholdSpin = new DragAdjustDoubleSpinBox(this);
    m_processThresholdSpin->setRange(5.0, 100.0);
    m_processThresholdSpin->setSingleStep(1.0);
    m_processThresholdSpin->setDecimals(0);
    m_processThresholdSpin->setSuffix(QStringLiteral(" %"));
    addSetting(tr("프로세스 CPU 임계값"), m_processThresholdSpin);

    m_topNSpin = new DragAdjustSpinBox(this);
    m_topNSpin->setRange(5, 20);
    m_topNSpin->setSingleStep(1);
    addSetting(tr("Top N"), m_topNSpin);

    m_deltaMarginSpin = new DragAdjustDoubleSpinBox(this);
    m_deltaMarginSpin->setRange(0.0, 50.0);
    m_deltaMarginSpin->setSingleStep(1.0);
    m_deltaMarginSpin->setDecimals(0);
    m_deltaMarginSpin->setSuffix(QStringLiteral(" %"));
    m_deltaMarginSpin->setToolTip(tr("0이면 상대 급증 감지를 끕니다."));
    addSetting(tr("상대 급증 마진"), m_deltaMarginSpin);

    settingsRow->addStretch();
    root->addLayout(settingsRow);

    m_summaryLabel = new QLabel(tr("시스템 CPU: — · 최고: —"), this);
    m_summaryLabel->setObjectName(QStringLiteral("spikeWatchSummary"));
    root->addWidget(m_summaryLabel);

    m_sectionSplitter = new QSplitter(Qt::Vertical, this);
    UiResizeHandle::configureSplitter(m_sectionSplitter);

    auto* processPane = new QWidget(m_sectionSplitter);
    auto* processLayout = new QVBoxLayout(processPane);
    processLayout->setContentsMargins(0, 0, 0, 0);
    processLayout->setSpacing(6);
    m_processTable = new QTableWidget(processPane);
    m_processTable->setColumnCount(4);
    m_processTable->setHorizontalHeaderLabels(
        {tr("순위"), tr("프로세스"), tr("PID"), tr("CPU %")});
    m_processTable->horizontalHeader()->setStretchLastSection(true);
    m_processTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_processTable->setAlternatingRowColors(true);
    m_processTable->setIconSize(QSize(16, 16));
    m_processTable->verticalHeader()->setDefaultSectionSize(24);
    processLayout->addWidget(m_processTable);
    m_sectionSplitter->addWidget(processPane);

    auto* culpritPane = new QWidget(m_sectionSplitter);
    auto* culpritLayout = new QVBoxLayout(culpritPane);
    culpritLayout->setContentsMargins(0, 0, 0, 0);
    culpritLayout->setSpacing(6);
    auto* culpritTitle = new QLabel(tr("범인 추정 (의심 순위)"), culpritPane);
    culpritLayout->addWidget(culpritTitle);
    m_culpritSummaryLabel = new QLabel(tr("감시를 시작하면 스파이크·CPU 패턴을 분석해 의심 프로세스를 표시합니다."),
                                       culpritPane);
    m_culpritSummaryLabel->setObjectName(QStringLiteral("spikeWatchCulpritSummary"));
    culpritLayout->addWidget(m_culpritSummaryLabel);
    m_culpritTable = new QTableWidget(culpritPane);
    m_culpritTable->setColumnCount(5);
    m_culpritTable->setHorizontalHeaderLabels(
        {tr("순위"), tr("프로세스"), tr("PID"), tr("의심도"), tr("근거")});
    m_culpritTable->horizontalHeader()->setStretchLastSection(true);
    m_culpritTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_culpritTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_culpritTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_culpritTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_culpritTable->setAlternatingRowColors(true);
    m_culpritTable->setIconSize(QSize(16, 16));
    m_culpritTable->verticalHeader()->setDefaultSectionSize(24);
    m_culpritTable->setMinimumHeight(80);
    culpritLayout->addWidget(m_culpritTable);
    m_sectionSplitter->addWidget(culpritPane);

    auto* logPane = new QWidget(m_sectionSplitter);
    auto* logLayout = new QVBoxLayout(logPane);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(6);
    auto* logTitle = new QLabel(tr("스파이크 이벤트"), logPane);
    logLayout->addWidget(logTitle);
    m_eventLog = new QTextEdit(logPane);
    m_eventLog->setReadOnly(true);
    m_eventLog->setLineWrapMode(QTextEdit::WidgetWidth);
    m_eventLog->setMinimumHeight(80);
    logLayout->addWidget(m_eventLog);
    m_sectionSplitter->addWidget(logPane);

    m_sectionSplitter->setStretchFactor(0, 2);
    m_sectionSplitter->setStretchFactor(1, 1);
    m_sectionSplitter->setStretchFactor(2, 1);
    m_sectionSplitter->setSizes({280, 180, 180});
    root->addWidget(m_sectionSplitter, 1);

    m_hintLabel = new HintLabel(
        tr("CPU 급증은 마우스 끊김의 원인 후보이며, DWM·입력 훅·디스크 등 다른 원인도 있습니다. "
           "범인 추정은 스파이크 1위·직접 트리거·고부하 샘플을 가중 합산한 휴리스틱이며, "
           "확정 진단이 아닙니다. "
           "관리자 권한이 없으면 일부 프로세스 CPU는 측정되지 않을 수 있습니다. "
           "감시 중에도 PIPBONG·감시 도구 자체 CPU가 포함됩니다."),
        this);
    m_hintLabel->setWordWrap(true);
    root->addWidget(m_hintLabel);

    connect(m_startButton, &QPushButton::clicked, this, &SpikeWatchDialog::onStartClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &SpikeWatchDialog::onStopClicked);
    connect(m_clearLogButton, &QPushButton::clicked, this, &SpikeWatchDialog::onClearLogClicked);
    connect(m_copyLogButton, &QPushButton::clicked, this, &SpikeWatchDialog::onCopyLogClicked);
}

void SpikeWatchDialog::loadPersistedState() {
    QSettings settings;
    restoreGeometry(settings.value(QLatin1String(kGeometrySettingsKey)).toByteArray());
    if (m_sectionSplitter) {
        const QByteArray splitterState =
            settings.value(QLatin1String(kSectionSplitterSettingsKey)).toByteArray();
        if (!splitterState.isEmpty()) {
            m_sectionSplitter->restoreState(splitterState);
        }
    }

    m_intervalSpin->setValue(settings.value(QLatin1String(kIntervalSettingsKey), kDefaultIntervalMs).toInt());
    m_systemThresholdSpin->setValue(
        settings.value(QLatin1String(kSystemThresholdSettingsKey), 70.0).toDouble());
    m_processThresholdSpin->setValue(
        settings.value(QLatin1String(kProcessThresholdSettingsKey), 40.0).toDouble());
    m_topNSpin->setValue(settings.value(QLatin1String(kTopNSettingsKey), kDefaultTopN).toInt());
    m_deltaMarginSpin->setValue(settings.value(QLatin1String(kDeltaMarginSettingsKey), 15.0).toDouble());
    Q_UNUSED(settings.value(QLatin1String(kCooldownSettingsKey), kDefaultCooldownMs));
}

void SpikeWatchDialog::persistState() {
    QSettings settings;
    settings.setValue(QLatin1String(kGeometrySettingsKey), saveGeometry());
    if (m_sectionSplitter) {
        settings.setValue(QLatin1String(kSectionSplitterSettingsKey), m_sectionSplitter->saveState());
    }
    settings.setValue(QLatin1String(kIntervalSettingsKey), m_intervalSpin->value());
    settings.setValue(QLatin1String(kSystemThresholdSettingsKey), m_systemThresholdSpin->value());
    settings.setValue(QLatin1String(kProcessThresholdSettingsKey), m_processThresholdSpin->value());
    settings.setValue(QLatin1String(kTopNSettingsKey), m_topNSpin->value());
    settings.setValue(QLatin1String(kDeltaMarginSettingsKey), m_deltaMarginSpin->value());
    settings.setValue(QLatin1String(kCooldownSettingsKey), kDefaultCooldownMs);
}

void SpikeWatchDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (m_worker) {
        m_worker->setFeatureRunningCallback(m_featureRunningCallback);
    }
}

void SpikeWatchDialog::closeEvent(QCloseEvent* event) {
    stopMonitoringAndWait();
    persistState();
    QDialog::closeEvent(event);
}

void SpikeWatchDialog::setMonitoringUiActive(bool active) {
    m_monitoringActive = active;
    m_startButton->setEnabled(!active);
    m_stopButton->setEnabled(active);
    m_intervalSpin->setEnabled(!active);
    m_systemThresholdSpin->setEnabled(!active);
    m_processThresholdSpin->setEnabled(!active);
    m_topNSpin->setEnabled(!active);
    m_deltaMarginSpin->setEnabled(!active);
}

void SpikeWatchDialog::stopMonitoringAndWait() {
    if (!m_worker || !m_monitoringActive) {
        return;
    }

    m_worker->requestStop();

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(m_worker, &CpuMonitorWorker::monitoringStopped, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(2000);
    loop.exec();
    setMonitoringUiActive(false);
}

CpuSpikeDetectorConfig SpikeWatchDialog::buildDetectorConfig() const {
    CpuSpikeDetectorConfig config;
    config.systemThresholdPercent = m_systemThresholdSpin->value();
    config.processThresholdPercent = m_processThresholdSpin->value();
    config.deltaMarginPercent = m_deltaMarginSpin->value();
    config.cooldownMs = kDefaultCooldownMs;
    return config;
}

void SpikeWatchDialog::onStartClicked() {
    if (!m_worker || m_monitoringActive) {
        return;
    }
    persistState();
    m_culpritAnalyzer.reset();
    m_culpritTable->setRowCount(0);
    m_culpritSummaryLabel->setText(
        tr("분석 중… 스파이크·CPU 패턴을 누적하고 있습니다."));
    setMonitoringUiActive(true);
    m_worker->setFeatureRunningCallback(m_featureRunningCallback);

    const int intervalMs = m_intervalSpin->value();
    const int topN = m_topNSpin->value();
    const CpuSpikeDetectorConfig config = buildDetectorConfig();
    QMetaObject::invokeMethod(
        m_worker,
        "startMonitoring",
        Qt::QueuedConnection,
        Q_ARG(int, intervalMs),
        Q_ARG(int, topN),
        Q_ARG(CpuSpikeDetectorConfig, config));
}

void SpikeWatchDialog::onStopClicked() {
    stopMonitoringAndWait();
}

void SpikeWatchDialog::onClearLogClicked() {
    m_eventLogPlainText.clear();
    m_eventLog->clear();
    m_culpritAnalyzer.reset();
    m_culpritTable->setRowCount(0);
    m_culpritSummaryLabel->setText(
        tr("감시를 시작하면 스파이크·CPU 패턴을 분석해 의심 프로세스를 표시합니다."));
}

void SpikeWatchDialog::onCopyLogClicked() {
    QString text = m_eventLogPlainText;
    const QString culpritReport = formatCulpritReportPlainText();
    if (!culpritReport.isEmpty()) {
        if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n'))) {
            text += QLatin1Char('\n');
        }
        text += culpritReport;
    }
    QApplication::clipboard()->setText(text);
}

void SpikeWatchDialog::onSampleReady(CpuSampleSnapshot snapshot) {
    if (!m_monitoringActive) {
        return;
    }

    if (!snapshot.systemReady) {
        m_summaryLabel->setText(tr("시스템 CPU: 준비 중… · 최고: —"));
        return;
    }

    QString topSummary = tr("—");
    if (!snapshot.topProcesses.isEmpty()) {
        const ProcessCpuEntry& top = snapshot.topProcesses.front();
        topSummary = QStringLiteral("%1 %2%")
                           .arg(top.name)
                           .arg(QString::number(top.cpuPercent, 'f', 1));
    }

    m_summaryLabel->setText(
        tr("시스템 CPU: %1% · 최고: %2")
            .arg(QString::number(snapshot.systemCpuPercent, 'f', 1))
            .arg(topSummary));

    m_processTable->setRowCount(snapshot.topProcesses.size());
    for (int row = 0; row < snapshot.topProcesses.size(); ++row) {
        const ProcessCpuEntry& entry = snapshot.topProcesses.at(row);
        m_processTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));

        auto* nameItem = new QTableWidgetItem(entry.name);
        QIcon icon = m_iconCache.iconForExecutablePath(entry.executablePath);
        if (icon.isNull()) {
            icon = m_iconCache.iconForProcessName(entry.name);
        }
        if (!icon.isNull()) {
            nameItem->setIcon(icon);
        }
        m_processTable->setItem(row, 1, nameItem);
        m_processTable->setItem(row, 2, new QTableWidgetItem(QString::number(entry.pid)));
        m_processTable->setItem(
            row,
            3,
            new QTableWidgetItem(QString::number(entry.cpuPercent, 'f', 1)));
    }

    m_culpritAnalyzer.recordSample(snapshot, m_processThresholdSpin->value());
    refreshCulpritTable();
}

QString SpikeWatchDialog::triggerKindDisplayName(SpikeTriggerKind kind) {
    switch (kind) {
    case SpikeTriggerKind::SystemAbsolute:
        return SpikeWatchDialog::tr("시스템 절대");
    case SpikeTriggerKind::ProcessAbsolute:
        return SpikeWatchDialog::tr("프로세스 절대");
    case SpikeTriggerKind::SystemRelative:
        return SpikeWatchDialog::tr("시스템 상대");
    case SpikeTriggerKind::ProcessRelative:
        return SpikeWatchDialog::tr("프로세스 상대");
    }
    return SpikeWatchDialog::tr("알 수 없음");
}

QString SpikeWatchDialog::formatSpikeEventText(const CpuSpikeEvent& event) const {
    const QString timestamp = event.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString kind = triggerKindDisplayName(event.triggerKind);
    const QString featureHint = event.pipbongFeatureRunning
                                    ? tr(" · PIPBONG 기능 실행 중")
                                    : QString();

    QStringList topLines;
    for (int i = 0; i < event.topProcesses.size(); ++i) {
        const ProcessCpuEntry& entry = event.topProcesses.at(i);
        topLines << QStringLiteral("  %1. %2 (PID %3) %4%")
                        .arg(i + 1)
                        .arg(entry.name)
                        .arg(entry.pid)
                        .arg(QString::number(entry.cpuPercent, 'f', 1));
    }

    return QStringLiteral("[%1] %2 · %3 · system %4%%5\n%6\n")
        .arg(timestamp, kind, event.triggerDetail)
        .arg(QString::number(event.systemCpuPercent, 'f', 1))
        .arg(featureHint)
        .arg(topLines.join(QLatin1Char('\n')));
}

void SpikeWatchDialog::appendSpikeLog(const CpuSpikeEvent& event) {
    const QString plain = formatSpikeEventText(event);
    m_eventLogPlainText += plain;
    if (!m_eventLogPlainText.endsWith(QLatin1Char('\n'))) {
        m_eventLogPlainText += QLatin1Char('\n');
    }

    const QString timestamp = event.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"));
    QString html = QStringLiteral("<div style=\"margin-bottom:6px;\">")
                   + QStringLiteral("<span style=\"color:#7dd3fc;\">[%1]</span> ").arg(htmlEscape(timestamp))
                   + QStringLiteral("<span style=\"color:#fbbf24;\">%1</span> · ").arg(htmlEscape(triggerKindDisplayName(event.triggerKind)))
                   + htmlEscape(event.triggerDetail)
                   + QStringLiteral("<br/>")
                   + QStringLiteral("<span style=\"color:#86efac;\">system %1%</span>").arg(QString::number(event.systemCpuPercent, 'f', 1));
    if (event.pipbongFeatureRunning) {
        html += QStringLiteral(" <span style=\"color:#c4b5fd;\">")
                + htmlEscape(tr("PIPBONG 기능 실행 중"))
                + QStringLiteral("</span>");
    }

    QStringList topHtml;
    for (int i = 0; i < event.topProcesses.size(); ++i) {
        const ProcessCpuEntry& entry = event.topProcesses.at(i);
        topHtml << htmlEscape(QStringLiteral("  %1. %2 (PID %3) %4%")
                                  .arg(i + 1)
                                  .arg(entry.name)
                                  .arg(entry.pid)
                                  .arg(QString::number(entry.cpuPercent, 'f', 1)));
    }
    html += QStringLiteral("<br/>") + topHtml.join(QStringLiteral("<br/>")) + QStringLiteral("</div>");
    m_eventLog->append(html);
}

void SpikeWatchDialog::onSpikeDetected(CpuSpikeEvent event) {
    if (!m_monitoringActive) {
        return;
    }
    appendSpikeLog(event);
    m_culpritAnalyzer.recordSpike(event);
    refreshCulpritTable();
}

void SpikeWatchDialog::refreshCulpritTable() {
    const QVector<CpuCulpritEntry> ranked = m_culpritAnalyzer.rankedCulprits(8);
    m_culpritTable->setRowCount(ranked.size());

    for (int row = 0; row < ranked.size(); ++row) {
        const CpuCulpritEntry& entry = ranked.at(row);
        m_culpritTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));

        auto* nameItem = new QTableWidgetItem(entry.name);
        QIcon icon = m_iconCache.iconForExecutablePath(entry.executablePath);
        if (icon.isNull()) {
            icon = m_iconCache.iconForProcessName(entry.name);
        }
        if (!icon.isNull()) {
            nameItem->setIcon(icon);
        }
        m_culpritTable->setItem(row, 1, nameItem);
        m_culpritTable->setItem(row, 2, new QTableWidgetItem(QString::number(entry.pid)));
        m_culpritTable->setItem(
            row,
            3,
            new QTableWidgetItem(QStringLiteral("%1%").arg(QString::number(entry.suspicionScore, 'f', 0))));
        m_culpritTable->setItem(row, 4, new QTableWidgetItem(entry.evidenceSummary));
    }

    updateCulpritSummary();
}

void SpikeWatchDialog::updateCulpritSummary() {
    const QVector<CpuCulpritEntry> ranked = m_culpritAnalyzer.rankedCulprits(1);
    if (ranked.isEmpty()) {
        if (m_monitoringActive) {
            m_culpritSummaryLabel->setText(tr("아직 의심 프로세스가 없습니다. 샘플을 더 수집 중…"));
        }
        return;
    }

    const CpuCulpritEntry& top = ranked.front();
    m_culpritSummaryLabel->setText(
        tr("1순위 의심: %1 (PID %2) · 의심도 %3% · %4")
            .arg(top.name)
            .arg(top.pid)
            .arg(QString::number(top.suspicionScore, 'f', 0))
            .arg(top.evidenceSummary));
}

QString SpikeWatchDialog::formatCulpritReportPlainText() const {
    const QVector<CpuCulpritEntry> ranked = m_culpritAnalyzer.rankedCulprits(8);
    if (ranked.isEmpty()) {
        return QString();
    }

    QStringList lines;
    lines << tr("--- 범인 추정 (의심 순위) ---");
    for (int i = 0; i < ranked.size(); ++i) {
        const CpuCulpritEntry& entry = ranked.at(i);
        lines << QStringLiteral("%1. %2 (PID %3) · 의심도 %4% · %5")
                     .arg(i + 1)
                     .arg(entry.name)
                     .arg(entry.pid)
                     .arg(QString::number(entry.suspicionScore, 'f', 0))
                     .arg(entry.evidenceSummary);
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void SpikeWatchDialog::onMonitoringStopped() {
    setMonitoringUiActive(false);
    refreshCulpritTable();
}
