#pragma once
#include <QObject>
#include <QPointer>
#include <functional>
class QMainWindow;
class QCheckBox;
class QMenu;
class QSystemTrayIcon;
class QTimer;
class CustomTitleBar;

// Window presentation and exit intent; document persistence is a host checkpoint.
class WindowShellController : public QObject {
    friend class WindowShellUiChecks;
public:
    struct Host {
        std::function<void()> hideAuxiliary,saveUiState;
        std::function<bool()> checkpoint;
        std::function<QString()> lastError;
        std::function<void(const QString&)> warning;
    };
    struct Ports {
        std::function<bool()> trayAvailable;
        std::function<void(bool)> trayVisible,topmost;
        std::function<void()> notifyTray,present;
    };
    WindowShellController(QMainWindow&,CustomTitleBar*,QCheckBox*,Host);
    WindowShellController(QMainWindow&,CustomTitleBar*,QCheckBox*,Host,Ports);
    ~WindowShellController() override;
    void applyClosePolicy();
    bool handleClose();
    void requestQuit();
    void restoreFromTray();
    // Caller has already completed the shutdown checkpoint (update path).
    void commitExit();
    void restoreTopmost();
    void toggleTopmost(bool);
    void shown();
    void setDocument(const QString& path,bool modified);
    void transient(const QString&,int timeoutMs);
    void persistent(const QString&);
    void clearTransient();
    QString transient()const{return m_transient;}
    QString persistent()const{return m_persistent;}
private:
    void ensureNativeTray();
    void refreshTitle();
    QPointer<QMainWindow> m_window;
    QPointer<CustomTitleBar> m_titleBar;
    QPointer<QCheckBox> m_topmost;
    QPointer<QSystemTrayIcon> m_tray;
    QPointer<QMenu> m_menu;
    QTimer* m_statusTimer;
    Host m_host;
    Ports m_ports;
    QString m_baseTitle,m_transient,m_persistent;
    bool m_explicitQuit=false,m_closing=false,m_exitCommitted=false,m_notified=false;
    bool m_previousQuitPolicy;
};
