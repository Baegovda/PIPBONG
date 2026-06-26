#pragma once

#include "ui/widgets/DragAdjustSpinMouse.h"

#include <QDoubleSpinBox>

class DragAdjustDoubleSpinBox : public QDoubleSpinBox {
    Q_OBJECT
public:
    explicit DragAdjustDoubleSpinBox(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    double dragStepSize(Qt::KeyboardModifiers modifiers) const;
    void applyDragDelta(int deltaPx, Qt::KeyboardModifiers modifiers);
    void onDragStarted();
    void enterTextInputMode(QMouseEvent* event, bool fromLineEdit);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseRelease(QMouseEvent* event, bool fromLineEdit);

    DragAdjustSpinMouseState m_mouse;
    double m_dragStartValue = 0.0;
};
