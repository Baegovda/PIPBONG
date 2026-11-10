#pragma once

/// Session execution state shared by the run model and its UI presentation.
/// Keeping this independent of widgets avoids pulling QTableWidget into sessions.
enum class WorkflowExecutionHighlight {
    None,
    Running,
    TriggerWatch,
    TriggerCooldown,
    ImageFindMiss,
    Success,
    Failed,
    ReturnToPrevious,
    ClipboardCopy,
    ClipboardPaste
};
