#pragma once

#include <QString>
#include <QStringList>

enum class CrashReportKind;

#ifdef _WIN32
struct _EXCEPTION_POINTERS;
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS;
#endif

namespace CrashManifestBuilder {

/// Builds machine-readable manifest JSON for AI agents.
QString buildManifestJson(CrashReportKind kind,
                          const QString& reason,
                          const QString& contextSnapshot,
                          const QString& lastUserAction,
#ifdef _WIN32
                          EXCEPTION_POINTERS* exceptionInfo,
                          struct _CONTEXT* fallbackContext,
#endif
                          bool hangReport);

QStringList buildSuspectedCauses(CrashReportKind kind,
                                 const QString& reason,
                                 const QString& contextSnapshot,
#ifdef _WIN32
                                 EXCEPTION_POINTERS* exceptionInfo,
#endif
                                 bool hangReport);

bool writeManifestFile(const QString& folderPath, const QString& manifestJson);

/// Maintains crash/incidents.json at the crash root (newest first, capped).
bool updateIncidentsIndex(const QString& crashRootPath,
                          const QString& incidentFolderName,
                          CrashReportKind kind,
                          const QString& reason,
                          const QStringList& suspectedCauses);

} // namespace CrashManifestBuilder
