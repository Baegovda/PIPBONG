#pragma once

#include <QSplitter>
#include <QSplitterHandle>

namespace UiResizeHandle {

class DampedSplitterHandle : public QSplitterHandle {
public:
    explicit DampedSplitterHandle(Qt::Orientation orientation, QSplitter* parent);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    int m_dragAnchor = 0;
};

class DampedSplitter : public QSplitter {
public:
    explicit DampedSplitter(Qt::Orientation orientation, QWidget* parent = nullptr);

protected:
    QSplitterHandle* createHandle() override;
};

} // namespace UiResizeHandle
