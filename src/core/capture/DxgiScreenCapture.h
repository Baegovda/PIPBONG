#pragma once

#include <opencv2/core.hpp>

#ifdef _WIN32
class DxgiScreenCapture {
public:
    /// Captures a physical-screen rectangle via DXGI Desktop Duplication (BGR). Empty on failure.
    static cv::Mat capturePhysicalRect(int x, int y, int width, int height);
    static void resetSession();
};
#endif
