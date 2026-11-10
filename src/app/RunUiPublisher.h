#pragma once
#include "app/FeatureRunSession.h"
#include <QObject>
#include <functional>
#include <map>
#include <memory>
#include <optional>

class QTimer;

/// Worker messages are reduced before entering the GUI queue. All session and
/// widget callbacks execute on this object's GUI thread; channels are per run.
class RunUiPublisher : public QObject {
public:
    struct Timing { int number; qint64 elapsedMs; qint64 averageMs; bool success; };
    struct HostCallbacks {
        // session/activeRepeatCount/isDisplayed are read-only queries.
        std::function<FeatureRunSession*(const std::string&)> session;
        std::function<int()> activeRepeatCount;
        std::function<bool(const FeatureRunSession*)> isDisplayed;
        std::function<void(std::optional<Timing>)> timing;
        std::function<void(const FeatureRunSession&, const QString&, bool)> log;
        std::function<void(FeatureRunSession&)> afterBatch;
        std::function<void()> refreshRunUi;
    };
    struct ChannelState;
    using Channel = std::shared_ptr<ChannelState>;
    explicit RunUiPublisher(HostCallbacks host, QObject* parent = nullptr);
    ~RunUiPublisher() override;
    Channel begin(const std::string& featureId);
    static void record(const Channel& channel, bool success, qint64 elapsedMs, const QString& message);
    void retire(const std::string& featureId);
    void flush(const std::string& featureId);
    void flushAll();
    void publish(FeatureRunSession& session, bool success, const QString& message);
    void syncTiming(const FeatureRunSession* session);
    void completeLoop(FeatureRunSession& session, bool success, const QString& message);
    static void accumulate(FeatureRunSession& session, bool success, qint64 elapsedOverrideMs = -1);
    quint64 queuedWakeCount() const;
private:
    struct Dispatcher;
    void arm(const Channel& channel);
    HostCallbacks m_host;
    QTimer* m_timer;
    std::map<std::string, Channel> m_channels;
    std::shared_ptr<Dispatcher> m_dispatch;
};
