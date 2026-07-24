#pragma once

#include <QWidget>

class QMainWindow;
class QMenuBar;
class QLabel;
class QPushButton;
class QCheckBox;

class CustomTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit CustomTitleBar(QMainWindow* window);

    QMenuBar* menuBar() const { return m_menuBar; }
    void syncFromWindowTitle();
    void setAlwaysOnTopCheckBox(QCheckBox* checkBox);
    bool hitTestCaptionDrag(const QPoint& globalPos) const;

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void updateTitleElide();
    void updateMaximizeButton();
    void beginNativeWindowDrag(const QPoint& globalPos);
    void toggleMaximize();

    QMainWindow* m_window = nullptr;
    QMenuBar* m_menuBar = nullptr;
    QLabel* m_appBadge = nullptr;
    QWidget* m_dragRegion = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_minimizeButton = nullptr;
    QPushButton* m_maximizeButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QCheckBox* m_alwaysOnTopCheck = nullptr;
    QString m_fullTitle;
};
