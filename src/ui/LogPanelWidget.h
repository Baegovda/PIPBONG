#pragma once

#include <QWidget>

class QTextEdit;
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
    void trimOldLines();

    QTextEdit* m_view = nullptr;
    QToolButton* m_clearButton = nullptr;
    int m_maxLines = 2000;
};

LogLineKind logKindForWorkflowMessage(const QString& message);
