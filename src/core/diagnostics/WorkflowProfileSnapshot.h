#pragma once

#include "app/ProgramSettings.h"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <vector>

class Feature;

struct WorkflowBlockSnapshotRow {
    int oneBasedIndex = 0;
    QString type;
    QString config;
};

struct BlockRunMeasuredRow {
    int oneBasedIndex = 0;
    QString type;
    QString config;
    int execCount = 0;
    int successCount = 0;
    int failCount = 0;
    qint64 totalDurationUs = 0;
    qint64 maxDurationUs = 0;
};

struct ProfileEventLite {
    qint64 relUs = 0;
    QString eventName;
    QString detail;
    qint64 durationUs = -1;
};

struct WorkflowProfileSnapshotData {
    QStringList featureSettings;
    std::vector<WorkflowBlockSnapshotRow> blocks;
    int blockCount = 0;
};

struct ProfileDiagnosisInput {
    QString featureName;
    QString runMode;
    QString endReason;
    QString startSource;
    int workflowBlockCount = 0;
    QStringList profileContext;
    QStringList featureSettings;
    std::vector<WorkflowBlockSnapshotRow> blockSnapshots;
    std::vector<BlockRunMeasuredRow> blockMeasured;
    std::vector<ProfileEventLite> sessionEvents;
    int loopBeginCount = 0;
    int loopEndCount = 0;
    int clickCount = 0;
    int physicalInputCount = 0;
    int imageFindPollCount = 0;
    int foregroundChangeCount = 0;
    qint64 maxLoopGapUs = 0;
};

WorkflowProfileSnapshotData captureWorkflowProfileSnapshot(const Feature& feature);

QStringList captureProfileContextSnapshot(const QString& profileId,
                                          const QString& profileName,
                                          const QString& mainTargetTitle,
                                          const QString& subTargetTitle,
                                          const ProgramSettings::ProfileSettings& settings);

QStringList buildProfileContextMarkdown(const QStringList& settings);
QStringList buildFeatureSettingsMarkdown(const QStringList& settings);
QStringList buildWorkflowBlocksMarkdown(const std::vector<WorkflowBlockSnapshotRow>& blocks);
QStringList buildBlockExecutionMarkdown(const std::vector<BlockRunMeasuredRow>& measured);
QStringList buildAutoDiagnosis(const ProfileDiagnosisInput& input);
