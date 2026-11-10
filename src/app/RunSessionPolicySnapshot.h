#pragma once
#include "app/FeatureRunSession.h"

inline SessionRunPolicyInput runSessionPolicyInput(const FeatureRunSession& session) {
    SessionRunPolicyInput input;
    input.runningMode = session.runningMode;
    input.triggerPhase = session.triggerPhase;
    input.repeatSession = session.repeatSession;
    input.holdRunActive = session.holdRunActive;
    input.repeatRemaining = session.repeatRemaining;
    input.engineRunning = (session.engine && session.engine->isRunning()) || session.holdKeyTapLaneActive;
    input.lockMouseDuringFirstLoopCount = session.lockMouseDuringFirstLoopCount;
    input.earlyLoopMouseLockReleased = session.earlyLoopMouseLockReleased;
    input.sessionIteration = session.sessionIteration;
    input.runLoopNumber = session.sessionContext ? session.sessionContext->runLoopNumber() : 0;
    return input;
}
