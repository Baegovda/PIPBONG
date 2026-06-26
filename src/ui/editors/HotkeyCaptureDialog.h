#pragma once

#include "core/input/HotkeyBinding.h"

#include <QDialog>
#include <QEvent>

class QLabel;
class QPushButton;

class HotkeyCaptureDialog : public QDialog {
    Q_OBJECT
public:
    explicit HotkeyCaptureDialog(const HotkeyBinding& current, QWidget* parent = nullptr);

    HotkeyBinding binding() const { return m_binding; }
    bool wasCleared() const { return m_cleared; }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void updateDisplay();
    void updateCaptureUi();
    void startCapture();
    void stopCapture();
    void applyCapturedBinding(int virtualKey, Qt::KeyboardModifiers modifiers);
    void applyBindingLabelIdleStyle();
    bool isInteractiveWidget(const QWidget* widget) const;

    HotkeyBinding m_binding;
    bool m_cleared = false;
    bool m_listeningForHotkey = false;
    QLabel* m_bindingLabel = nullptr;
};
