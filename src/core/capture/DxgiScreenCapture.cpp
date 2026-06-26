#include "core/capture/DxgiScreenCapture.h"

#ifdef _WIN32

#include <opencv2/imgproc.hpp>

#include <cstring>

#include <d3d11.h>
#include <dxgi1_2.h>

namespace {

constexpr UINT kAcquireTimeoutMs = 5;
constexpr UINT kInitialAcquireTimeoutMs = 250;

struct DxgiSession {
    HMONITOR monitor = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    ID3D11Texture2D* staging = nullptr;
    int desktopLeft = 0;
    int desktopTop = 0;
    int desktopWidth = 0;
    int desktopHeight = 0;
    cv::Mat cachedDesktopBgr;
    bool hasCachedDesktop = false;
    bool valid = false;
    bool frameAcquired = false;
};

DxgiSession g_session;

void releaseSession(DxgiSession& session) {
    if (session.frameAcquired && session.duplication) {
        session.duplication->ReleaseFrame();
        session.frameAcquired = false;
    }
    if (session.staging) {
        session.staging->Release();
        session.staging = nullptr;
    }
    if (session.duplication) {
        session.duplication->Release();
        session.duplication = nullptr;
    }
    if (session.context) {
        session.context->Release();
        session.context = nullptr;
    }
    if (session.device) {
        session.device->Release();
        session.device = nullptr;
    }
    session.monitor = nullptr;
    session.cachedDesktopBgr.release();
    session.hasCachedDesktop = false;
    session.valid = false;
}

bool ensureStaging(DxgiSession& session, int width, int height) {
    if (session.staging && session.desktopWidth == width && session.desktopHeight == height) {
        return true;
    }

    if (session.staging) {
        session.staging->Release();
        session.staging = nullptr;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    session.desktopWidth = width;
    session.desktopHeight = height;
    return SUCCEEDED(session.device->CreateTexture2D(&desc, nullptr, &session.staging));
}

bool initSessionForMonitor(HMONITOR monitor, DxgiSession& session) {
    if (session.valid && session.monitor == monitor) {
        return true;
    }

    releaseSession(session);

    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        return false;
    }

    IDXGIAdapter1* adapter = nullptr;
    IDXGIOutput1* output1 = nullptr;
    IDXGIAdapter1* matchedAdapter = nullptr;
    DXGI_OUTPUT_DESC matchedDesc{};

    for (UINT adapterIndex = 0; factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++adapterIndex) {
        for (UINT outputIndex = 0;; ++outputIndex) {
            IDXGIOutput* output = nullptr;
            if (adapter->EnumOutputs(outputIndex, &output) == DXGI_ERROR_NOT_FOUND) {
                break;
            }

            DXGI_OUTPUT_DESC desc{};
            output->GetDesc(&desc);
            if (desc.Monitor == monitor) {
                if (SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput1),
                                                     reinterpret_cast<void**>(&output1)))) {
                    matchedAdapter = adapter;
                    adapter->AddRef();
                    matchedDesc = desc;
                }
            }
            output->Release();
            if (output1) {
                break;
            }
        }
        adapter->Release();
        if (output1) {
            break;
        }
    }

    factory->Release();

    if (!output1 || !matchedAdapter) {
        if (output1) {
            output1->Release();
        }
        if (matchedAdapter) {
            matchedAdapter->Release();
        }
        return false;
    }

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    const D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const HRESULT deviceHr =
        D3D11CreateDevice(matchedAdapter,
                          D3D_DRIVER_TYPE_UNKNOWN,
                          nullptr,
                          0,
                          featureLevels,
                          1,
                          D3D11_SDK_VERSION,
                          &device,
                          &featureLevel,
                          &context);
    matchedAdapter->Release();

    if (FAILED(deviceHr)) {
        output1->Release();
        return false;
    }

    IDXGIOutputDuplication* duplication = nullptr;
    const HRESULT dupHr = output1->DuplicateOutput(device, &duplication);
    output1->Release();

    if (FAILED(dupHr)) {
        context->Release();
        device->Release();
        return false;
    }

    session.device = device;
    session.context = context;
    session.duplication = duplication;
    session.monitor = monitor;
    session.desktopLeft = matchedDesc.DesktopCoordinates.left;
    session.desktopTop = matchedDesc.DesktopCoordinates.top;
    session.desktopWidth =
        matchedDesc.DesktopCoordinates.right - matchedDesc.DesktopCoordinates.left;
    session.desktopHeight =
        matchedDesc.DesktopCoordinates.bottom - matchedDesc.DesktopCoordinates.top;
    session.valid = true;
    return true;
}

HMONITOR monitorForRect(int x, int y, int width, int height) {
    RECT rect{x, y, x + width, y + height};
    return MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
}

cv::Mat cropCachedDesktopBgr(const DxgiSession& session, int x, int y, int width, int height) {
    if (!session.hasCachedDesktop || session.cachedDesktopBgr.empty()) {
        return {};
    }

    const int srcLeft = x - session.desktopLeft;
    const int srcTop = y - session.desktopTop;
    if (srcLeft < 0 || srcTop < 0 || srcLeft + width > session.cachedDesktopBgr.cols
        || srcTop + height > session.cachedDesktopBgr.rows) {
        return {};
    }

    return session.cachedDesktopBgr(cv::Rect(srcLeft, srcTop, width, height)).clone();
}

bool updateDesktopCacheFromTexture(DxgiSession& session, ID3D11Texture2D* desktopTexture) {
    if (!ensureStaging(session, session.desktopWidth, session.desktopHeight)) {
        return false;
    }

    session.context->CopyResource(session.staging, desktopTexture);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT mapHr = session.context->Map(session.staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(mapHr)) {
        return false;
    }

    const cv::Mat bgraWrapper(session.desktopHeight,
                              session.desktopWidth,
                              CV_8UC4,
                              mapped.pData,
                              static_cast<size_t>(mapped.RowPitch));
    if (session.cachedDesktopBgr.rows != session.desktopHeight
        || session.cachedDesktopBgr.cols != session.desktopWidth) {
        session.cachedDesktopBgr.create(session.desktopHeight, session.desktopWidth, CV_8UC3);
    }
    cv::cvtColor(bgraWrapper, session.cachedDesktopBgr, cv::COLOR_BGRA2BGR);
    session.hasCachedDesktop = true;

    session.context->Unmap(session.staging, 0);
    return true;
}

} // namespace

void DxgiScreenCapture::resetSession() {
    releaseSession(g_session);
}

cv::Mat DxgiScreenCapture::capturePhysicalRect(int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) {
        return {};
    }

    const HMONITOR monitor = monitorForRect(x, y, width, height);
    if (!monitor || !initSessionForMonitor(monitor, g_session)) {
        return {};
    }

    if (g_session.frameAcquired && g_session.duplication) {
        g_session.duplication->ReleaseFrame();
        g_session.frameAcquired = false;
    }

    const UINT acquireTimeout =
        g_session.hasCachedDesktop ? kAcquireTimeoutMs : kInitialAcquireTimeoutMs;

    IDXGIResource* resource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    const HRESULT acquireHr =
        g_session.duplication->AcquireNextFrame(acquireTimeout, &frameInfo, &resource);

    if (acquireHr == DXGI_ERROR_WAIT_TIMEOUT) {
        return cropCachedDesktopBgr(g_session, x, y, width, height);
    }
    if (acquireHr == DXGI_ERROR_ACCESS_LOST) {
        resetSession();
        return {};
    }
    if (FAILED(acquireHr) || !resource) {
        return cropCachedDesktopBgr(g_session, x, y, width, height);
    }
    g_session.frameAcquired = true;

    ID3D11Texture2D* desktopTexture = nullptr;
    const HRESULT queryHr =
        resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&desktopTexture));
    resource->Release();
    if (FAILED(queryHr) || !desktopTexture) {
        g_session.duplication->ReleaseFrame();
        g_session.frameAcquired = false;
        return cropCachedDesktopBgr(g_session, x, y, width, height);
    }

    const int srcLeft = x - g_session.desktopLeft;
    const int srcTop = y - g_session.desktopTop;
    if (srcLeft < 0 || srcTop < 0 || srcLeft + width > g_session.desktopWidth
        || srcTop + height > g_session.desktopHeight) {
        desktopTexture->Release();
        g_session.duplication->ReleaseFrame();
        g_session.frameAcquired = false;
        return {};
    }

    if (!updateDesktopCacheFromTexture(g_session, desktopTexture)) {
        desktopTexture->Release();
        g_session.duplication->ReleaseFrame();
        g_session.frameAcquired = false;
        return cropCachedDesktopBgr(g_session, x, y, width, height);
    }

    desktopTexture->Release();
    g_session.duplication->ReleaseFrame();
    g_session.frameAcquired = false;

    return cropCachedDesktopBgr(g_session, x, y, width, height);
}

#endif
