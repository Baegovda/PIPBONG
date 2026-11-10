#pragma once
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include <memory>
#include <QFutureWatcher>
#include "storage/ProfilePackageImport.h"
class QWidget;
class QMenuBar;
class QTimer;
class ProfileManager;
class ProfilePackageController;
class ProjectWorkspaceController;

// Owns File/Help menu commands and modal document/package orchestration.
// Document persistence remains in ProjectWorkspaceController.
class DocumentActionsController : public QObject {
    friend class DocumentActionsChecks;
public:
    struct Host {
        // These two queries are read-only.
        std::function<bool()> ready;
        std::function<QString()> directory;
        std::function<bool(bool)> checkpoint;
        std::function<bool()> prepareImport;
        std::function<void()> imported,stopSessions,update,exit;
        std::function<void(const QString&,bool)> log;
        std::function<void(const QString&)> status;
    };
    struct Ports {
        std::function<QString(QWidget*,const QString&,const QString&,const QString&,bool)> file;
        std::function<void(QWidget*,const QString&,const QString&)> error;
        std::function<bool(QWidget*,const QString&,const QString&,const std::function<bool()>&)> restore;
        // Worker callback must own its data; it may outlive this controller.
        std::function<std::shared_ptr<PreparedProfileImport>(const ProfileImportRequest&)> prepare;
    };
    DocumentActionsController(QWidget&,ProfileManager&,ProjectWorkspaceController&,ProfilePackageController&,Host);
    ~DocumentActionsController() override;
    bool busy()const{return m_busy;}
    void installMenus(QMenuBar&);
    void newProject();void openProject();void saveProject();void saveProjectAs();
    void importPackage();void exportPackage();void restoreProject();
private:
    struct Ticket;struct Action;
    Ticket ticket()const;
    void report(const Ticket&,const QString&,const QString&)const;
    QString selectFile(const Ticket&,const QString&,const QString&,const QString&,bool);
    void completeImport();
    void retireImport();
    QPointer<QWidget> m_owner;
    ProfileManager& m_profiles;
    QPointer<ProjectWorkspaceController> m_workspace;
    QPointer<ProfilePackageController> m_packages;
    Host m_host;Ports m_ports;
    bool m_busy=false;
    QFutureWatcher<std::shared_ptr<PreparedProfileImport>> m_importWatcher;
    QTimer* m_importRetry=nullptr;
    std::shared_ptr<Ticket> m_importSource;
    std::shared_ptr<PreparedProfileImport> m_preparedImport;
    bool m_importNotice=false;
};
