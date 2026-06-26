#pragma once

#include <QColor>
#include <QFrame>
#include <QString>

class QEvent;
class QLabel;

struct TargetWindowDetailData {
    QString hwnd;
    QString title;
    QString className;
    QString frameBounds;
    QString clientSize;
    QString monitor;
    QString processName;
    QString processPath;
    QString stateText;
    bool minimized = false;
    bool visible = true;
    int monitorDpi = 0;
};

class TargetWindowDetailPanel : public QFrame {
    Q_OBJECT
public:
    explicit TargetWindowDetailPanel(QWidget* parent = nullptr);

    void showMessage(const QString& message);
    void showDetails(const TargetWindowDetailData& data);

protected:
    void changeEvent(QEvent* event) override;

private:
    void refreshDetailText();
    void updateThemeColors();
    void setLabelTextColor(QLabel* label, const QColor& color, int fontSizePx, bool bold) const;
    QColor stateColorFor(const TargetWindowDetailData& data) const;

    QWidget* m_messagePage = nullptr;
    QLabel* m_messageLabel = nullptr;
    QWidget* m_detailsPage = nullptr;
    QLabel* m_primaryLine = nullptr;
    QLabel* m_secondaryLine = nullptr;
    TargetWindowDetailData m_lastDetailData;
    bool m_updatingTheme = false;
};
