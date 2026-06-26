#pragma once

#include <QWidget>

class QPropertyAnimation;

// Pill track with a sliding circular thumb between two labeled sides (e.g. 고정 / 랜덤).
class AnimatedTwoWaySwitch : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal thumbPosition READ thumbPosition WRITE setThumbPosition)

public:
    explicit AnimatedTwoWaySwitch(const QString& leftLabel,
                                  const QString& rightLabel,
                                  QWidget* parent = nullptr);

    bool isRightSelected() const { return m_rightSelected; }
    void setRightSelected(bool right, bool animate = true);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void rightSelectedChanged(bool right);

public slots:
    void setThumbPosition(qreal position);
    qreal thumbPosition() const { return m_thumbPosition; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void animateTo(bool right);
    QRectF thumbRectForPosition(qreal position) const;
    QRectF trackRect() const;

    QString m_leftLabel;
    QString m_rightLabel;
    bool m_rightSelected = false;
    qreal m_thumbPosition = 0.0;
    QPropertyAnimation* m_animation = nullptr;
};
