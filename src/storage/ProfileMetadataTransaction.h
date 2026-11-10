#pragma once
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include <functional>
struct ProfileMetadataFile { QString path; QByteArray bytes; };
struct ProfileMetadataResult { bool success=false, recoveryRequired=false; QString message; };
/// Small metadata files only; never copies/removes profile assets.
ProfileMetadataResult writeProfileMetadata(const QString& root,const QVector<ProfileMetadataFile>& files,
                                           const std::function<void(int)>& afterCommitForTest={});
ProfileMetadataResult recoverProfileMetadata(const QString& root);
