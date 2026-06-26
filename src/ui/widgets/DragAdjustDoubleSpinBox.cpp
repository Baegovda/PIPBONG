#include "ui/widgets/DragAdjustDoubleSpinBox.h"

#include <QLineEdit>
#include <QMouseEvent>

DragAdjustDoubleSpinBox::DragAdjustDoubleSpinBox(QWidget* parent)
    : QDoubleSpinBox(parent) {
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setCursor(Qt::SizeHorCursor);
    if (QLineEdit* edit = lineEdit()) {
        edit->installEventFilter(this);
        edit->setCursor(Qt::SizeHorCursor);
    }
}

double DragAdjustDoubleSpinBox::dragStepSize(Qt::KeyboardModifiers modifiers) const {
    const double base = singleStep();
    if (modifiers.testFlag(Qt::ControlModifier)) {
        return base * 100.0;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        return base * 10.0;
    }
    return base;
}

void DragAdjustDoubleSpinBox::applyDragDelta(int deltaPx, Qt::KeyboardModifiers modifiers) {
    const double step = dragStepSize(modifiers);
    setValue(m_dragStartValue + static_cast<double>(deltaPx) * step);
}

void DragAdjustDoubleSpinBox::onDragStarted() {
    if (QLineEdit* edit = lineEdit()) {
        edit->deselect();
        edit->clearFocus();
    }
}

void DragAdjustDoubleSpinBox::enterTextInputMode(QMouseEvent* event, bool fromLineEdit) {
    m_mouse = {};

    setFocus(Qt::MouseFocusReason);
    QLineEdit* edit = lineEdit();
    if (!edit) {
        return;
    }

    edit->setFocus(Qt::MouseFocusReason);
    edit->unsetCursor();

    if (fromLineEdit) {
        const QPoint local = edit->mapFromGlobal(event->globalPosition().toPoint());
        const int position = edit->cursorPositionAt(local);
        edit->setSelection(position, 0);
        return;
    }

    edit->selectAll();
}

bool DragAdjustDoubleSpinBox::handleMouseMove(QMouseEvent* event) {
    const bool wasDragging = m_mouse.dragging;
    if (!dragAdjustContinueMove(m_mouse, event, this)) {
        return false;
    }
    if (!wasDragging && m_mouse.dragging) {
        onDragStarted();
    }
    if (m_mouse.dragging) {
        applyDragDelta(dragAdjustDeltaPx(m_mouse, event), event->modifiers());
        event->accept();
        return true;
    }
    return true;
}

bool DragAdjustDoubleSpinBox::handleMouseRelease(QMouseEvent* event, bool fromLineEdit) {
    const DragAdjustReleaseResult result = dragAdjustFinishRelease(m_mouse, event, this);
    if (result == DragAdjustReleaseResult::DragFinished) {
        return true;
    }
    if (result == DragAdjustReleaseResult::ClickToEdit) {
        enterTextInputMode(event, fromLineEdit);
        return true;
    }
    return false;
}

bool DragAdjustDoubleSpinBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == lineEdit()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            if (dragAdjustBeginPress(m_mouse, static_cast<QMouseEvent*>(event))) {
                m_dragStartValue = value();
                return true;
            }
            break;
        case QEvent::MouseMove:
            if (handleMouseMove(static_cast<QMouseEvent*>(event))) {
                return true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (handleMouseRelease(static_cast<QMouseEvent*>(event), true)) {
                return true;
            }
            break;
        default:
            break;
        }
    }
    return QDoubleSpinBox::eventFilter(watched, event);
}

void DragAdjustDoubleSpinBox::mousePressEvent(QMouseEvent* event) {
    if (dragAdjustBeginPress(m_mouse, event)) {
        m_dragStartValue = value();
        return;
    }
    QDoubleSpinBox::mousePressEvent(event);
}

void DragAdjustDoubleSpinBox::mouseMoveEvent(QMouseEvent* event) {
    if (handleMouseMove(event)) {
        return;
    }
    QDoubleSpinBox::mouseMoveEvent(event);
}

void DragAdjustDoubleSpinBox::mouseReleaseEvent(QMouseEvent* event) {
    if (handleMouseRelease(event, false)) {
        return;
    }
    QDoubleSpinBox::mouseReleaseEvent(event);
}
