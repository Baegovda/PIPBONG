#pragma once

#include <QStringList>
#include <QWidget>

class QTextEdit;
class QTimer;
class QToolButton;

enum class LogLineKind {
    Info,
    Success,
    Warning,
    Error,
    Accent,
};

class LogPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogPanelWidget(QWidget* parent = nullptr);

    void appendLine(LogLineKind kind, const QString& text);
    void appendSessionLine(const QString& featureName, LogLineKind kind, const QString& text);
    void clearLog();

private:
    QString colorForKind(LogLineKind kind) const;
    void appendHtml(const QString& html);
    void flushPendingHtml();
    void trimOldLines();

    QTextEdit* m_view = nullptr;
    QToolButton* m_clearButton = nullptr;
    QTimer* m_flushTimer = nullptr;
    QStringList m_pendingHtml;
    int m_maxLines = 1000;
    int m_maxPendingLines = 48;
};

LogLineKind logKindForWorkflowMessage(const QString& message);
