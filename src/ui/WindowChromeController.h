#pragma once
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <functional>
#include <optional>
class QMainWindow;
class CustomTitleBar;

// Geometry is physical, half-open Win32 screen coordinates. Native changes
// occur only on the product's show path; hit testing performs reads only.
class WindowChromeController : public QObject {
    friend class WindowChromeChecks;
public:
    using Handle=quintptr;
    struct Bounds {qint64 left=0,top=0,right=0,bottom=0;};
    struct State {bool maximized=false,width=true,height=true;int border=10;};
    enum class Hit {Left,Right,Top,Bottom,TopLeft,TopRight,BottomLeft,BottomRight,Caption};
    struct Ports {
        std::function<Handle()> handle;
        std::function<std::optional<Bounds>(Handle)> rectangle;
        std::function<void(Handle)> frame,round;
        std::function<std::optional<QPoint>(Handle,QPoint)> clientPoint;
        std::function<qreal()> ratio;
        // Qt logical window-client coordinates, never native global pixels.
        std::function<bool(QPointF)> caption;
    };
    WindowChromeController(QMainWindow&,CustomTitleBar*,Ports={},QObject* parent=nullptr);
    static std::optional<Hit> resizeHit(Bounds,QPoint,State);
    static bool contains(Bounds,QPoint);
    static std::optional<QPointF> logicalClientPoint(QPoint physicalClient,qreal ratio);
    void shown();
    bool nativeEvent(const QByteArray&,void* message,qintptr* result);
private:
    QPointer<QMainWindow> m_window;
    Ports m_ports;
};
