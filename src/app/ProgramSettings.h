#pragma once

/// Program-wide preferences persisted in QSettings (not project JSON).
class ProgramSettings {
public:
    static bool autoSelectRunningFeature();
    static void setAutoSelectRunningFeature(bool enabled);

    static bool showWorkflowRunFeedback();
    static void setShowWorkflowRunFeedback(bool enabled);
};
