#pragma once

#include <QIcon>
#include <QPixmap>
#include <QPointF>
#include <QWidget>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

class TitleBarIconEasterEgg : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal popProgress READ popProgress WRITE setPopProgress)
    Q_PROPERTY(qreal returnProgress READ returnProgress WRITE setReturnProgress)

public:
    static void play(const QIcon& icon, QWidget* anchor = nullptr);

    qreal popProgress() const { return m_popProgress; }
    void setPopProgress(qreal progress);

    qreal returnProgress() const { return m_returnProgress; }
    void setReturnProgress(qreal progress);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    explicit TitleBarIconEasterEgg(const QIcon& icon, QWidget* anchor = nullptr);

    void paintIcon(QPainter& painter) const;

    QPixmap m_icon;
    QPointF m_anchorCenter;
    qreal m_popProgress = 0.0;
    qreal m_returnProgress = 0.0;
};
