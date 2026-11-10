#pragma once
#include <memory>
#include <mutex>
#include <utility>

// Publication and capture registration serialize. Once registered, every
// capture holds a lease until its pixels have been copied. Native hide calls
// happen after snapshotting and releasing the mutex, never under it.
class FeedbackCaptureBarrier {
    struct State {std::recursive_mutex mutex;size_t captures=0;};
    std::shared_ptr<State> m_state=std::make_shared<State>();
public:
    class Lease {
        friend class FeedbackCaptureBarrier;
        std::shared_ptr<State> state;
        explicit Lease(std::shared_ptr<State> s):state(std::move(s)){++state->captures;}
    public:
        Lease()=default;
        Lease(const Lease&)=delete;Lease& operator=(const Lease&)=delete;
        Lease(Lease&& b)noexcept:state(std::move(b.state)){}
        Lease& operator=(Lease&&)=delete;
        ~Lease(){if(state){std::lock_guard<std::recursive_mutex> lock(state->mutex);--state->captures;}}
    };
    template<class F> auto synchronized(F f)const {
        std::lock_guard<std::recursive_mutex> lock(m_state->mutex);return f();
    }
    template<class F> bool publish(F f)const {
        return synchronized([&]{if(m_state->captures)return false;f();return true;});
    }
    template<class F> auto capture(F snapshot)const {
        return synchronized([&]{Lease lease(m_state);auto value=snapshot();return std::make_pair(std::move(lease),std::move(value));});
    }
};
