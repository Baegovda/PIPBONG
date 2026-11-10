#include "app/InputInterruptController.h"
#include "app/FeatureRunSession.h"
#include "app/UserInputInterruptMonitor.h"
#include "model/Feature.h"

InputInterruptController::InputInterruptController()
    : InputInterruptController({
        [](const Registration& value) {
            UserInputInterruptMonitor::instance().registerSession(value.featureId, value.mode,
                value.hotkey, value.context, value.keyboard);
        },
        [](const std::string& id) { UserInputInterruptMonitor::instance().unregisterSession(id); }
    }) {}

void InputInterruptController::sync(FeatureRunSession& session) {
    const auto* feature = m_host.feature ? m_host.feature(session) : nullptr;
    if (!feature || !session.sessionContext || feature->userInputInterruptMode() == UserInputInterruptMode::None
        || session.userStopRequested || session.sessionContext->shouldStop()) {
        if (m_ports.unsubscribe) m_ports.unsubscribe(session.featureId);
        return;
    }
    if (m_ports.subscribe) m_ports.subscribe({session.featureId, feature->userInputInterruptMode(),
        feature->hotkey(), session.sessionContext, feature->runMode() != FeatureRunMode::Hold});
}

InputInterruptController::Outcome InputInterruptController::interrupt(
    FeatureRunSession& session, const std::shared_ptr<ExecutionContext>& expected) {
    if (!expected || session.sessionContext != expected || session.userStopRequested || expected->shouldStop())
        return Outcome::Ignored;
    const auto* feature = m_host.feature ? m_host.feature(session) : nullptr;
    if (!feature || feature->userInputInterruptMode() == UserInputInterruptMode::None) return Outcome::Ignored;
    const QString name = QString::fromStdString(feature->name());
    Outcome outcome;
    if (feature->userInputInterruptMode() == UserInputInterruptMode::Stop) {
        const bool trigger = session.runningMode == FeatureRunMode::Trigger;
        session.disarmPersistedTrigger |= trigger;
        session.userStopRequested = true;
        session.repeatSession = false;
        session.holdRunActive = false;
        ++session.holdRepeatGeneration;
        expected->requestStop();
        if (session.engine) session.engine->stop();
        if (m_ports.unsubscribe) m_ports.unsubscribe(session.featureId);
        if (trigger && m_host.disarm) m_host.disarm(session);
        outcome = Outcome::Stopped;
    } else {
        expected->togglePaused();
        outcome = expected->isPaused() ? Outcome::Paused : Outcome::Resumed;
    }
    if (m_host.changed) m_host.changed(session, name, outcome);
    return outcome;
}
