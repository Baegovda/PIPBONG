#include "ui/LogPanelWidget.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString htmlEscape(const QString& text) {
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return escaped;
}

QString coloredSpan(const QString& color, const QString& text) {
    return QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(color, htmlEscape(text));
}

} // namespace

LogLineKind logKindForWorkflowMessage(const QString& message) {
    if (message.contains(QStringLiteral("오류")) || message.contains(QStringLiteral("초과"))
        || message.contains(QStringLiteral("유효하지 않"))) {
        return LogLineKind::Error;
    }
    if (message.contains(QStringLiteral("실패")) || message.contains(QStringLiteral("없음"))
        || message.contains(QStringLiteral("중지")) || message.contains(QStringLiteral("찾지 못"))) {
        return LogLineKind::Warning;
    }
    if (message.contains(QStringLiteral("찾음")) || message.contains(QStringLiteral("완료"))
        || message.contains(QStringLiteral("성공")) || message.contains(QStringLiteral("발동"))
        || message.contains(QStringLiteral("입력 ·"))) {
        return LogLineKind::Success;
    }
    return LogLineKind::Info;
}

LogPanelWidget::LogPanelWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("logPanel"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(6);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("logPanelHeader"));
    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(2, 0, 2, 0);
    headerRow->setSpacing(8);

    auto* title = new QLabel(tr("실행 로그"), header);
    title->setObjectName(QStringLiteral("logPanelTitle"));

    m_clearButton = new QToolButton(header);
    m_clearButton->setObjectName(QStringLiteral("logPanelClearButton"));
    m_clearButton->setText(tr("지우기"));
    m_clearButton->setAutoRaise(true);
    m_clearButton->setCursor(Qt::PointingHandCursor);
    connect(m_clearButton, &QToolButton::clicked, this, &LogPanelWidget::clearLog);

    headerRow->addWidget(title);
    headerRow->addStretch();
    headerRow->addWidget(m_clearButton);

    m_view = new QTextEdit(this);
    m_view->setObjectName(QStringLiteral("logPanelView"));
    m_view->setReadOnly(true);
    m_view->setUndoRedoEnabled(false);
    m_view->setLineWrapMode(QTextEdit::WidgetWidth);
    m_view->setMinimumHeight(28);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->document()->setDefaultStyleSheet(QStringLiteral("p { margin: 0; }"));

    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(50);
    connect(m_flushTimer, &QTimer::timeout, this, &LogPanelWidget::flushPendingHtml);

    outer->addWidget(header);
    outer->addWidget(m_view, 1);

    setStyleSheet(QStringLiteral(
        "QWidget#logPanel { background: transparent; }"
        "QLabel#logPanelTitle {"
        "  color: #8b949e;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  letter-spacing: 0.3px;"
        "}"
        "QToolButton#logPanelClearButton {"
        "  color: #6e7681;"
        "  font-size: 11px;"
        "  padding: 2px 8px;"
        "  border-radius: 4px;"
        "}"
        "QToolButton#logPanelClearButton:hover {"
        "  color: #c9d1d9;"
        "  background-color: #21262d;"
        "}"
        "QTextEdit#logPanelView {"
        "  background-color: #0d1117;"
        "  color: #3fb950;"
        "  border: 1px solid #30363d;"
        "  border-radius: 8px;"
        "  padding: 8px 10px;"
        "  font-family: 'Cascadia Mono', 'Consolas', 'D2Coding', monospace;"
        "  font-size: 12px;"
        "  selection-background-color: #264f78;"
        "  selection-color: #e6edf3;"
        "}"));
}

QString LogPanelWidget::colorForKind(LogLineKind kind) const {
    switch (kind) {
    case LogLineKind::Success:
        return QStringLiteral("#56d364");
    case LogLineKind::Warning:
        return QStringLiteral("#e3b341");
    case LogLineKind::Error:
        return QStringLiteral("#ff7b72");
    case LogLineKind::Accent:
        return QStringLiteral("#79c0ff");
    case LogLineKind::Info:
    default:
        return QStringLiteral("#3fb950");
    }
}

void LogPanelWidget::appendHtml(const QString& html) {
    while (m_pendingHtml.size() >= m_maxPendingLines) {
        m_pendingHtml.removeFirst();
    }
    m_pendingHtml.append(html);
    if (!m_flushTimer->isActive()) {
        m_flushTimer->start();
    }
}

void LogPanelWidget::flushPendingHtml() {
    if (m_pendingHtml.isEmpty()) {
        return;
    }
    const QString html = m_pendingHtml.join(QStringLiteral("<br/>")) + QStringLiteral("<br/>");
    m_pendingHtml.clear();
    m_view->moveCursor(QTextCursor::End);
    m_view->insertHtml(html);
    trimOldLines();
    if (QScrollBar* bar = m_view->verticalScrollBar()) {
        bar->setValue(bar->maximum());
    }
}

void LogPanelWidget::trimOldLines() {
    QTextDocument* doc = m_view->document();
    while (doc->blockCount() > m_maxLines) {
        QTextCursor cursor(doc->firstBlock());
        cursor.select(QTextCursor::BlockUnderCursor);
        cursor.removeSelectedText();
        if (!doc->isEmpty()) {
            cursor.deleteChar();
        }
    }
}

void LogPanelWidget::appendLine(LogLineKind kind, const QString& text) {
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    QString html = coloredSpan(QStringLiteral("#6e7681"), timestamp + QStringLiteral("  "));
    html += coloredSpan(colorForKind(kind), text);
    appendHtml(html);
}

void LogPanelWidget::appendSessionLine(const QString& featureName,
                                       LogLineKind kind,
                                       const QString& text) {
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    QString html = coloredSpan(QStringLiteral("#6e7681"), timestamp + QStringLiteral("  "));
    if (!featureName.isEmpty()) {
        html += coloredSpan(QStringLiteral("#79c0ff"), QStringLiteral("[") + featureName
                                                                    + QStringLiteral("] "));
    }
    html += coloredSpan(colorForKind(kind), text);
    appendHtml(html);
}

void LogPanelWidget::clearLog() {
    m_flushTimer->stop();
    m_pendingHtml.clear();
    m_view->clear();
}

void LogPanelWidget::setMaxLines(int maxLines) {
    m_maxLines = std::max(1, maxLines);
    flushPendingHtml();
    trimOldLines();
}

int LogPanelWidget::maxLines() const {
    return m_maxLines;
}
