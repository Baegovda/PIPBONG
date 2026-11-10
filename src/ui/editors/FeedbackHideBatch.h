#pragma once
#include <QtGlobal>
#include <cstdint>
#include <functional>
#include <vector>
struct FeedbackHideRequest {quintptr window=0;std::uint64_t token=0;};
struct FeedbackHidePorts {
    std::function<std::uint64_t()> clock;
    std::function<bool(quintptr)> owned;
    std::function<bool(const FeedbackHideRequest&,unsigned)> hide;
};
inline bool hideFeedbackBatch(const std::vector<FeedbackHideRequest>& windows,const FeedbackHidePorts& ports) {
    const auto start=ports.clock();
    for(const auto& request:windows) {
        if(!ports.owned(request.window))continue;
        const auto elapsed=ports.clock()-start;
        if(elapsed>=200 || !ports.hide(request,static_cast<unsigned>(200-elapsed)))return false;
    }
    return true;
}
