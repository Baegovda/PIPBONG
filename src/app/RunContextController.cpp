#include "app/RunContextController.h"
#include "app/FeatureRunSession.h"
#include "model/Feature.h"
#include "core/capture/ScreenCapture.h"

void RunContextController::apply(FeatureRunSession& session, const Feature& feature) {
    auto& context = *session.sessionContext;
    session.pointerVisualFeedback = feature.pointerVisualFeedback();
    context.setPointerVisualFeedback(session.pointerVisualFeedback);
    session.restoreMousePositionOnEnd = feature.restoreMousePositionOnEnd();
    session.lockMouseToScreenCenterDuringRun = feature.lockMouseToScreenCenterDuringRun();
    session.lockMouseToCurrentPositionDuringRun = feature.lockMouseToCurrentPositionDuringRun();
    session.lockMouseDuringFirstLoopCount = feature.lockMouseDuringFirstLoopCount();
    session.unlockMouseOnBlockFailureCount = feature.unlockMouseOnBlockFailureCount();
    const bool monitoring = session.runningMode == FeatureRunMode::Trigger
                         && session.triggerPhase == TriggerSessionPhase::Monitoring;
    const bool failOnMiss = !monitoring && feature.infiniteExitAfterConsecutiveMisses() > 0
        && (session.runningMode == FeatureRunMode::RepeatInfinite || session.runningMode == FeatureRunMode::Hold);
    context.setImageFindMaxMissAttempts(failOnMiss ? 1 : 0);
    context.setRoiCorrectionSession(feature.roiCorrectionSessionEligible(), feature.roiCorrection());
    context.setFeatureRoiCorrectionExpandPercent(feature.roiCorrectionExpandPercent());
    context.setRunLoopNumber(session.sessionIteration + 1);
    if (!session.workerGates) session.workerGates = std::make_shared<RunWorkerGates>();
    const auto gates = session.workerGates;
    context.setScopedTargetPollGate([gates] { return gates->targetAllowed(); });
    if (monitoring) context.setTriggerMonitorYieldGate([gates] { return gates->captureAllowed(); });
    else context.clearTriggerMonitorYieldGate();
}

std::wstring RunContextController::processPath(const TargetBinding& binding, FeatureCaptureTargetScope scope,
                                              const std::wstring& targetTitle) {
    const auto title=QString::fromStdWString(targetTitle).trimmed();
    const auto main=QString::fromStdWString(binding.mainTitle).trimmed(),subTitle=QString::fromStdWString(binding.subTitle).trimmed();
    if(scope==FeatureCaptureTargetScope::Auto && !main.isEmpty() && title.compare(main,Qt::CaseInsensitive)==0)
        return binding.mainProcessPath;
    if(scope==FeatureCaptureTargetScope::Auto && !subTitle.isEmpty() && title.compare(subTitle,Qt::CaseInsensitive)==0)
        return binding.subProcessPath;
    if(scope==FeatureCaptureTargetScope::Auto && !main.isEmpty() && !subTitle.isEmpty()
        && title.contains(main,Qt::CaseInsensitive) && title.contains(subTitle,Qt::CaseInsensitive))
        return subTitle.length()>main.length()?binding.subProcessPath:binding.mainProcessPath;
    const bool sub = scope == FeatureCaptureTargetScope::SubOnly
        || (scope == FeatureCaptureTargetScope::Auto && !binding.subTitle.empty()
            && QString::fromStdWString(targetTitle).contains(QString::fromStdWString(binding.subTitle), Qt::CaseInsensitive));
    return sub ? binding.subProcessPath : binding.mainProcessPath;
}

WorkflowRunPreparer RunContextController::prepare(Preparation preparation, std::function<void()> activate) {
    if (!activate) activate = [title = preparation.targetTitle, process = preparation.targetProcessPath] {
#ifdef _WIN32
        ScreenCapture::activateTargetWindow(title, process);
#endif
    };
    return [plan = std::move(preparation), activate = std::move(activate)] {
        auto run = plan.run;
        if (!run.workflow || !run.context) return run;
        if (plan.configureTarget) {
            run.context->setTargetWindowBindingForWorker(plan.targetTitle, plan.targetProcessPath);
            run.context->setProjectDirectory(plan.projectDirectory);
            if (plan.configureMonitor) {
                run.context->setTriggerMonitorBlockIndex(plan.triggerBlockIndex);
                run.context->setImageFindPrimedBlockIndex(-1);
            }
        }
        if (plan.activateTarget && !run.context->shouldStop()
            && (!plan.requireTargetGate || run.context->scopedTargetPollAllowed())
            && (!plan.burstActivation || !plan.burstActivation->exchange(true))) activate();
        return run;
    };
}
