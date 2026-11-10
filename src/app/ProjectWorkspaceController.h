#pragma once
#include "app/ProgramSettings.h"
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class Project;
class ProfileManager;
class ProfilePackageController;
class QTimer;

/// Owns active document, persistence, profile staging and cache/package policy.
/// No widget or MainWindow dependency; hosts bind the installed model to views.
class ProjectWorkspaceController : public QObject {
public:
    struct State {
        // Authoritative document; views may pin a retired model until rebinding.
        std::shared_ptr<Project> project;
        QString profileId,path,directory;
        bool modified=false;
    };
    struct Staged {
        std::unique_ptr<Project> project;
        QString profileId,path,directory;
        ProgramSettings::ProfileSettings settings;
        bool applySettings=false, fresh=false, clearTarget=false;
    };
    struct Change { bool fresh=false,clearTarget=false,quiet=false,restoreTriggers=false; };
    enum class InstallMode { Guarded, ValidatedTransition };
    struct HostCallbacks {
        std::function<bool()> mayReplace, mayWrite, maySchedule;
        std::function<void()> beforeReplace,changed;
        std::function<void(const Change&)> installed;
        std::function<void(const QString&)> directoryChanged;
        std::function<QString()> currentDirectory;
        std::function<void(const QString&)> status,warning;
    };
    ProjectWorkspaceController(ProfileManager& profiles, QString fallbackPath, QObject* parent=nullptr);
    ~ProjectWorkspaceController() override;
    State& state() { return m_state; }
    void setHost(HostCallbacks host) { m_host=std::move(host); }
    void setPackages(ProfilePackageController* packages) { m_packages=packages; }
    QTimer* autoSaveTimer() const { return m_autoSave; }
    void markModified();
    bool ensurePath();
    bool save(bool quiet=false);
    bool saveAs(const QString& path);
    bool checkpoint(bool quiet=true);
    bool saveSettings();
    bool saveCenterPinOptions(const QString& profileId,quint64 generation,bool main,bool sub);
    quint64 generation() const { return m_generation; }
    bool commitTargetBinding(const QString& profileId, quint64 generation, bool sub,
                             const QString& title, const QString& processPath);
    void scheduleSeal();
    bool seal(bool synchronous);
    bool load(const QString& path,bool quiet=false);
    bool loadActive(bool quiet=false,bool restoreTriggers=true);
    bool createEmpty();
    std::unique_ptr<Staged> stageProfile(const QString& profileId);
    bool install(std::unique_ptr<Staged> staged,bool quiet,bool restoreTriggers,InstallMode mode=InstallMode::Guarded);
    QString lastError() const { return m_error; }
private:
    struct WriteTicket;
    WriteTicket writeTicket()const;
    bool syncCanonical();
    std::unique_ptr<Staged> stageFile(const QString& profileId,const QString& path);
    QString directory() const;
    QString ownerProfile() const;
    bool fail(const QString& text);
    bool saveSettingsValue(ProgramSettings::ProfileSettings settings);
    quint64 m_generation=1;
    bool write(const QString& path,bool updatePath,bool canonicalOnly=false);
    static bool sameSettings(const ProgramSettings::ProfileSettings& a,const ProgramSettings::ProfileSettings& b);
    ProfileManager& m_profiles;
    QString m_fallbackPath,m_error;
    State m_state;
    HostCallbacks m_host;
    ProfilePackageController* m_packages=nullptr;
    QTimer* m_autoSave;
    bool m_settingsValid=false;
    bool m_profileSnapshotPending=false;
    QString m_settingsProfile;
    ProgramSettings::ProfileSettings m_settings;
};
