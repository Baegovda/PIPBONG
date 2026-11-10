#pragma once
#include <QRect>
#include <QString>
#include "core/capture/TargetWindowBinding.h"
#include <memory>
#include <map>
#include <vector>
#include <limits>
#include <cstdint>
#include <string>

struct PointerFeedbackFrame {
    quintptr window=0;
    unsigned long process=0;
    QRect clientBounds; // physical screen pixels, not DWM frame bounds
    bool valid()const {
        return window && process && clientBounds.isValid()
            && qint64(clientBounds.x())+clientBounds.width()<=std::numeric_limits<int>::max()
            && qint64(clientBounds.y())+clientBounds.height()<=std::numeric_limits<int>::max();
    }
    bool operator==(const PointerFeedbackFrame& b)const {
        return window==b.window && process==b.process && clientBounds==b.clientBounds;
    }
};
inline bool ownedPointerFeedbackWindow(unsigned long process,unsigned long currentProcess,
                                      std::uint64_t expected,std::uint64_t actual,bool classMatches) {
    return process && process==currentProcess && expected && expected==actual && classMatches;
}
struct PointerFeedbackOwner {
    QString profile;
    std::string feature;
    std::shared_ptr<const void> lifetime;
    bool same(const PointerFeedbackOwner& b)const {
        return profile==b.profile && feature==b.feature
            && !lifetime.owner_before(b.lifetime) && !b.lifetime.owner_before(lifetime);
    }
};
inline bool pointerFeedbackBindingMatches(const std::pair<std::wstring,std::wstring>& binding,
                                         const std::wstring& title,const std::wstring& path) {
    return targetWindowBindingMatches(binding,title,path);
}
// GUI-owned identity book. Native HWNDs are separate; retired tokens must be
// destroyed before showing the returned successor. A context lease prevents
// address reuse from aliasing a still-visible pulse belonging to an older run.
class PointerFeedbackLayers {
public:
    struct Layer { PointerFeedbackOwner owner;PointerFeedbackFrame frame; };
    struct Update {std::uint64_t token=0;std::vector<std::uint64_t> retired;};
    Update observe(PointerFeedbackOwner owner,const PointerFeedbackFrame& frame) {
        Update result;if(!frame.valid() || m_next==std::numeric_limits<std::uint64_t>::max())return result;
        for(auto it=m_layers.begin();it!=m_layers.end();++it)if(it->second.owner.same(owner)) {
            if(it->second.frame==frame){result.token=it->first;return result;}
            result.retired.push_back(it->first);m_layers.erase(it);break;
        }
        if(m_layers.size()>=16){result.retired.push_back(m_layers.begin()->first);m_layers.erase(m_layers.begin());}
        result.token=++m_next;m_layers.emplace(result.token,Layer{std::move(owner),frame});return result;
    }
    void retire(std::uint64_t token){m_layers.erase(token);}
    const Layer* find(std::uint64_t token)const {auto it=m_layers.find(token);return it==m_layers.end()?nullptr:&it->second;}
    size_t size()const{return m_layers.size();}
private:
    std::uint64_t m_next=0;
    std::map<std::uint64_t,Layer> m_layers;
};
