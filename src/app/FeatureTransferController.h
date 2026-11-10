#pragma once
#include <QString>
#include <functional>
#include <memory>

class Project;
class Feature;
class ProfileManager;
class FeatureLibraryManager;

/// Owns feature/library/profile mutations. Host callbacks publish changes; this
/// controller has no widget, window or running-engine dependency. Calls run on
/// the owner thread; getters are read-only, publication callbacks may retire it.
class FeatureTransferController {
public:
    struct HostCallbacks {
        std::function<std::shared_ptr<Project>()> activeProject;
        std::function<void(const QString& profileId,const QString& featureId)> stopFeature;
        std::function<void(const QString& selectedId)> activeChanged;
        std::function<void(const QString& profileId)> profileSaved;
        std::function<void()> libraryChanged;
        std::function<void(const QString&)> warning;
        std::function<QString()> activeDirectory; // read-only runtime asset root
        std::function<quint64()> generation; // read-only document replacement revision
    };
    FeatureTransferController(ProfileManager& profiles, FeatureLibraryManager& library, HostCallbacks host);
    ~FeatureTransferController();
    FeatureTransferController(const FeatureTransferController&)=delete;
    FeatureTransferController& operator=(const FeatureTransferController&)=delete;
    void retire();
    bool remove(const QString& profileId, const QString& featureId);
    bool importLibrary(const QString& entryId, const QString& profileId, int insertIndex,
                       bool publish = true, QString* insertedId = nullptr);
    bool saveLibrary(const QString& featureId, const QString& profileId, const QString& name = QString(), bool publish = true);
    bool copy(const QString& featureId, const QString& sourceId, const QString& targetId, int insertIndex,
              bool publish = true, QString* insertedId = nullptr);
private:
    struct State;
    struct Operation;
    struct Target;
    std::shared_ptr<State> m_state;
    ProfileManager& m_profiles;
    FeatureLibraryManager& m_library;
    HostCallbacks m_host;
};
