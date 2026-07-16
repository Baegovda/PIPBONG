#pragma once

#include <QColor>
#include <QFrame>
#include <QString>
#include <QVector>
#include <QPair>

class QEvent;
class QLabel;
class QLayout;
class QResizeEvent;
class QWidget;

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
    bool subTarget = false;
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
    void showGlobalDefaultProfile();
    void showStoredTargetBinding(const QString& title,
                                 const QString& processName,
                                 const QString& processPath);

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class DetailLayoutDensity {
        Comfortable,
        Compact,
        Narrow,
    };

    void refreshDetailText();
    void refreshGlobalDefaultProfileText();
    void refreshStoredTargetBindingText(const QString& title,
                                        const QString& processName,
                                        const QString& processPath);
    void refreshVisibleDetailText();
    void updateAdaptiveLayout();
    void rebuildTitleRowLayout();
    DetailLayoutDensity densityForWidth(int contentWidthPx) const;
    QString formatFieldsHtml(const QVector<QPair<QString, QString>>& fields,
                             DetailLayoutDensity density,
                             const QColor& muted,
                             const QColor& text) const;
    void updateThemeColors();
    void setLabelTextColor(QLabel* label, const QColor& color, int fontSizePx, bool bold) const;
    QColor stateColorFor(const TargetWindowDetailData& data) const;

    QWidget* m_messagePage = nullptr;
    QLabel* m_messageLabel = nullptr;
    QWidget* m_detailsPage = nullptr;
    QWidget* m_titleRowWidget = nullptr;
    QLayout* m_titleRowLayout = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_primaryLine = nullptr;
    QLabel* m_secondaryLine = nullptr;
    TargetWindowDetailData m_lastDetailData;
    QString m_storedBindingTitle;
    QString m_storedBindingProcessName;
    QString m_storedBindingProcessPath;
    bool m_updatingTheme = false;
    bool m_layoutReady = false;
    DetailLayoutDensity m_layoutDensity = DetailLayoutDensity::Comfortable;
    bool m_globalDefaultProfileMode = false;
    bool m_storedTargetBindingMode = false;
};
