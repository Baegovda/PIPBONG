#pragma once
#include "app/FeatureBatchController.h"
#include "app/FeatureLibraryManager.h"
#include <QObject>
#include <QPointer>
#include <optional>
class QWidget;
class QMimeData;
class Feature;
class FeatureListPanel;
class ProfileManager;
class ProjectWorkspaceController;

// Owns library dialogs, drag commands, result presentation and preview publication.
// Domain mutation/undo/partial results remain in FeatureBatch/TransferController.
class FeatureLibraryActionsController : public QObject {
    friend class FeatureLibraryActionsChecks;
public:
    struct Host {
        std::function<bool()> ready; // read-only
        std::function<QString()> previewId; // read-only
        std::function<void(const QString&,std::shared_ptr<Feature>)> preview;
        std::function<void(const QString&,int)> status;
        std::function<void(const QString&)> warning;
    };
    struct Ports {
        std::function<std::optional<QString>(QWidget*,const QString&)> name;
        std::function<QString(QWidget*,const std::vector<FeatureLibraryManager::Entry>&)> choose;
        std::function<bool(QWidget*,const QString&,const QString&)> confirm;
        std::function<void(QWidget*,const QString&,const QString&,bool)> notice;
    };
    FeatureLibraryActionsController(QWidget&,ProfileManager&,FeatureLibraryManager&,ProjectWorkspaceController&,FeatureBatchController&,FeatureListPanel&,Host);
    void save(const QString& featureId);
    void chooseImport();
    bool importEntries(const QStringList&);
    void remove(const QStringList&);
    void preview(const QString&);
    void refreshPanel();
    void dropOnFeatureList(const QMimeData*,int);
    void dropOnLibrary(const QMimeData*);
    void dropOnProfile(const QString&,const QMimeData*);
    void reorder(int from,int to);
    void reorder(const QList<int>& rows,int insertIndex);
private:
    struct Ticket;struct Action;
    Ticket ticket()const;
    static void cancelled(const Ticket&);
    static void present(const Ticket&,const FeatureBatchController::Result&,const QString& targetName={});
    static bool executeImport(const Ticket&,const QStringList&);
    void presentOrder(FeatureBatchController::OrderResult);
    QPointer<QWidget> m_owner;
    ProfileManager& m_profiles;
    FeatureLibraryManager& m_library;
    QPointer<ProjectWorkspaceController> m_workspace;
    QPointer<FeatureBatchController> m_batches;
    QPointer<FeatureListPanel> m_view;
    Host m_host;Ports m_ports;
    bool m_busy=false;
    quint64 m_previewEpoch=0;
};
