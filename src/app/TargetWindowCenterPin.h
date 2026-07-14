#pragma once

/// Keeps the designated target window centered on its current monitor (Win32).
class TargetWindowCenterPin {
public:
    /// Move the target window to the monitor center when it has drifted. Returns true if repositioned.
    static bool sync();
};
