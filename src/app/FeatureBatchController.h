#pragma once
#include <QObject>
#include <QStringList>
#include <functional>
#include <memory>

class Project;
class ProfileManager;
class FeatureLibraryManager;
class FeatureTransferController;

/// Batch ownership/checkpoint/order policy. Dialogs and view publication remain host-owned.
class FeatureBatchController : public QObject {
public:
    enum class Kind { Import, Save, Copy, Remove };
    struct Request {
        Kind kind = Kind::Import;
        QStringList ids;
        QString sourceProfile, targetProfile, name, undoReason;
        int insertIndex = -1;
        // Read-only caller/source guard, after the event-pumping checkpoint.
        std::function<bool()> validate;
    };
    struct Result {
        Kind kind = Kind::Import;
        QStringList completed, failed, names;
        QString lastFeatureId;
        int requested = 0;
        bool cancelled = false;
    };
    enum class OrderResult { Changed, Unchanged, Failed, Cancelled };
    struct Host {
        std::function<bool()> canBegin;
        std::function<quint64()> generation;
        std::function<Project*()> activeProject;
        std::function<bool(const QString&)> checkpoint;
        std::function<void(const QString&)> activeChanged;
        std::function<void()> libraryChanged;
    };
    FeatureBatchController(ProfileManager&,FeatureLibraryManager&,FeatureTransferController&,Host,QObject* parent=nullptr);
    Result run(const Request&);
    OrderResult reorder(QList<int> rows,int insertIndex,const std::function<bool()>& validate={});
    OrderResult reorderOne(int from,int to,const std::function<bool()>& validate={});
private:
    OrderResult saveOrder(const QStringList& before,const QStringList& after,const QString& reason,const std::function<bool()>& validate);
    ProfileManager& m_profiles;
    FeatureLibraryManager& m_library;
    FeatureTransferController& m_transfers;
    Host m_host;
    std::shared_ptr<bool> m_busy = std::make_shared<bool>(false);
};
