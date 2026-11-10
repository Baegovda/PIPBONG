#pragma once
#include <QString>
#include <memory>
class QTemporaryDir;
struct ProfileImportRequest {QString root,id,name,source;bool recovery=false;};
struct PreparedProfileImport {
    QString root,id,name,directory,package,workspace,archive,error;
    QString projectTitle;
    std::shared_ptr<QTemporaryDir> temporary;
    bool ready=false;
};
// Worker preparation owns only values and a private staging directory. It never
// changes the profile registry, selected profile or a live workspace.
std::shared_ptr<PreparedProfileImport> prepareProfileImport(const ProfileImportRequest&);
// Validate/rebase only an unpacked profile's metadata. Relative template paths
// now resolve under the destination root; absolute/external assets are separate.
bool rebaseUnpackedProfile(const QString& workspace,const QString& destination,QString* error=nullptr);
bool recoverProfilePackage(const QString& root,const QString& id,const QString& package);
