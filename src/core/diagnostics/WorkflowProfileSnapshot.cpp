#include "core/diagnostics/WorkflowProfileSnapshot.h"

#include "core/workflow/Block.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/ClickBlock.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include "core/workflow/blocks/TextBlock.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "model/Feature.h"
#include "model/FeatureCaptureTargetScope.h"
#include "model/FeatureRunMode.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace {

QString escapeMd(QString value) {
    value.replace(QLatin1Char('|'), QLatin1Char('/'));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return value;
}

QString formatMs(qint64 micros, int decimals = 1) {
    return QString::number(micros / 1000.0, 'f', decimals);
}

QString captureTargetScopeLabel(FeatureCaptureTargetScope scope) {
    switch (scope) {
    case FeatureCaptureTargetScope::MainOnly:
        return QStringLiteral("MainOnly");
    case FeatureCaptureTargetScope::SubOnly:
        return QStringLiteral("SubOnly");
    case FeatureCaptureTargetScope::Auto:
    default:
        return QStringLiteral("Auto");
    }
}

QString mouseButtonLabel(MouseButton button) {
    switch (button) {
    case MouseButton::Right:
        return QStringLiteral("Right");
    case MouseButton::Middle:
        return QStringLiteral("Middle");
    case MouseButton::Back:
        return QStringLiteral("Back");
    case MouseButton::Forward:
        return QStringLiteral("Forward");
    case MouseButton::WheelUp:
        return QStringLiteral("WheelUp");
    case MouseButton::WheelDown:
        return QStringLiteral("WheelDown");
    case MouseButton::Left:
    default:
        return QStringLiteral("Left");
    }
}

QString clickActionLabel(ClickAction action) {
    switch (action) {
    case ClickAction::MoveOnly:
        return QStringLiteral("MoveOnly");
    case ClickAction::Down:
        return QStringLiteral("Down");
    case ClickAction::Up:
        return QStringLiteral("Up");
    case ClickAction::Tap:
    default:
        return QStringLiteral("Tap");
    }
}

QString clickTargetLabel(ClickBlock::ClickTarget target) {
    switch (target) {
    case ClickBlock::ClickTarget::Fixed:
        return QStringLiteral("Fixed");
    case ClickBlock::ClickTarget::CurrentPosition:
        return QStringLiteral("CurrentPosition");
    case ClickBlock::ClickTarget::LastMatch:
    default:
        return QStringLiteral("LastMatch");
    }
}

QString blockConfigDetail(const Block& block) {
    switch (block.type()) {
    case BlockType::Wait: {
        const auto& wait = static_cast<const WaitBlock&>(block);
        if (wait.randomRange) {
            return QStringLiteral("random %1~%2 ms").arg(wait.minMs).arg(wait.maxMs);
        }
        return QStringLiteral("%1 ms").arg(wait.ms);
    }
    case BlockType::Click: {
        const auto& click = static_cast<const ClickBlock&>(block);
        QString detail = QStringLiteral("%1 %2×%3")
                             .arg(clickTargetLabel(click.target))
                             .arg(mouseButtonLabel(click.button))
                             .arg(click.count);
        if (click.target == ClickBlock::ClickTarget::Fixed
            || (click.target == ClickBlock::ClickTarget::LastMatch && click.lastMatchRelativeOffset)) {
            detail += QStringLiteral(" @(%1,%2)").arg(click.x).arg(click.y);
        }
        if (click.action != ClickAction::Tap) {
            detail += QStringLiteral(" action=%1").arg(clickActionLabel(click.action));
        }
        return detail;
    }
    case BlockType::ImageFind: {
        const auto& imageFind = static_cast<const ImageFindBlock&>(block);
        QString detail = QStringLiteral("templates=%1 poll=%2ms thr=%3")
                             .arg(imageFind.templatePaths.size())
                             .arg(imageFind.pollIntervalMs)
                             .arg(imageFind.threshold, 0, 'f', 2);
        if (!imageFind.customRegionsWindowPercent.empty() || !imageFind.customRegions.empty()) {
            const int roiCount = imageFind.customRegionsWindowPercent.empty()
                                     ? static_cast<int>(imageFind.customRegions.size())
                                     : static_cast<int>(imageFind.customRegionsWindowPercent.size());
            detail += QStringLiteral(" roi=%1").arg(roiCount);
        }
        if (imageFind.templateMatchMode == ImageFindTemplateMatchMode::All) {
            detail += QStringLiteral(" match=All");
        }
        return detail;
    }
    case BlockType::KeyPress:
        return QString::fromStdString(block.summary());
    case BlockType::Text: {
        const auto& text = static_cast<const TextBlock&>(block);
        QString preview = QString::fromStdString(text.text);
        preview.replace(QLatin1Char('\n'), QLatin1Char(' '));
        if (preview.length() > 48) {
            preview = preview.left(45) + QStringLiteral("...");
        }
        return preview.isEmpty() ? QStringLiteral("(empty)") : preview;
    }
    default:
        return QString::fromStdString(block.summary());
    }
}

qint64 configuredWaitMs(const WorkflowBlockSnapshotRow& row) {
    static const QRegularExpression fixedMs(QStringLiteral(R"(^(\d+)\s*ms$)"));
    static const QRegularExpression randomMs(QStringLiteral(R"(random\s+(\d+)~(\d+)\s*ms)"));
    const QRegularExpressionMatch fixedMatch = fixedMs.match(row.config);
    if (fixedMatch.hasMatch()) {
        return fixedMatch.captured(1).toLongLong();
    }
    const QRegularExpressionMatch randomMatch = randomMs.match(row.config);
    if (randomMatch.hasMatch()) {
        const qint64 minMs = randomMatch.captured(1).toLongLong();
        const qint64 maxMs = randomMatch.captured(2).toLongLong();
        return (minMs + maxMs) / 2;
    }
    return -1;
}

qint64 relUsForEvent(const std::vector<ProfileEventLite>& events, const char* name) {
    for (const ProfileEventLite& event : events) {
        if (event.eventName == QLatin1String(name)) {
            return event.relUs;
        }
    }
    return -1;
}

qint64 relUsForLastEvent(const std::vector<ProfileEventLite>& events, const char* name) {
    qint64 relUs = -1;
    for (const ProfileEventLite& event : events) {
        if (event.eventName == QLatin1String(name)) {
            relUs = event.relUs;
        }
    }
    return relUs;
}

} // namespace

WorkflowProfileSnapshotData captureWorkflowProfileSnapshot(const Feature& feature) {
    WorkflowProfileSnapshotData data;
    const Workflow& workflow = feature.workflow();
    data.blockCount = static_cast<int>(workflow.blocks().size());

    data.featureSettings
        << QStringLiteral("run_mode=%1").arg(QString::fromStdString(featureRunModeToString(feature.runMode())));
    if (feature.runMode() == FeatureRunMode::RepeatCount) {
        data.featureSettings << QStringLiteral("repeat_count=%1").arg(feature.repeatCount());
    }
    if (feature.runMode() == FeatureRunMode::Trigger) {
        data.featureSettings << QStringLiteral("trigger_cooldown_ms=%1").arg(feature.triggerCooldownMs());
        data.featureSettings << QStringLiteral("trigger_background=%1")
                                    .arg(feature.triggerRunWithoutTargetForeground() ? QStringLiteral("yes")
                                                                                     : QStringLiteral("no"));
    }
    if (feature.supportsLoopInterval()) {
        if (feature.loopIntervalRandomRange()) {
            data.featureSettings << QStringLiteral("loop_interval_ms=random %1~%2")
                                        .arg(feature.loopIntervalMinMs())
                                        .arg(feature.loopIntervalMaxMs());
        } else {
            data.featureSettings << QStringLiteral("loop_interval_ms=%1").arg(feature.loopIntervalMs());
        }
    }
    if (feature.infiniteExitAfterConsecutiveMisses() > 0) {
        data.featureSettings << QStringLiteral("exit_after_consecutive_misses=%1")
                                    .arg(feature.infiniteExitAfterConsecutiveMisses());
    }
    data.featureSettings << QStringLiteral("capture_target_scope=%1")
                                .arg(captureTargetScopeLabel(feature.captureTargetScope()));
    if (feature.requireScopedTargetForeground()) {
        data.featureSettings << QStringLiteral("require_scoped_target_foreground=yes");
    }
    if (feature.roiCorrectionSessionEligible() && feature.roiCorrection()) {
        data.featureSettings << QStringLiteral("roi_correction=yes expand=%1%")
                                    .arg(feature.roiCorrectionExpandPercent());
    }
    if (feature.lockMouseDuringFirstLoopCount() > 0) {
        data.featureSettings << QStringLiteral("early_loop_mouse_lock=%1 loops")
                                    .arg(feature.lockMouseDuringFirstLoopCount());
    }
    if (feature.lockMouseToScreenCenterDuringRun()) {
        data.featureSettings << QStringLiteral("lock_mouse_center=yes");
    }
    if (feature.lockMouseToCurrentPositionDuringRun()) {
        data.featureSettings << QStringLiteral("lock_mouse_position=yes");
    }

    data.blocks.reserve(workflow.blocks().size());
    for (int i = 0; i < static_cast<int>(workflow.blocks().size()); ++i) {
        const Block& block = *workflow.blocks()[static_cast<size_t>(i)];
        WorkflowBlockSnapshotRow row;
        row.oneBasedIndex = i + 1;
        row.type = QString::fromStdString(blockTypeToString(block.type()));
        row.config = blockConfigDetail(block);
        data.blocks.push_back(std::move(row));
    }
    return data;
}

QStringList buildFeatureSettingsMarkdown(const QStringList& settings) {
    QStringList lines;
    lines << QStringLiteral("## Feature settings (session snapshot)");
    lines << QString();
    if (settings.isEmpty()) {
        lines << QStringLiteral("_No feature session snapshot._");
        return lines;
    }
    lines << QStringLiteral("| Setting | Value |");
    lines << QStringLiteral("| --- | --- |");
    for (const QString& entry : settings) {
        const int eq = entry.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            lines << QStringLiteral("| %1 | |").arg(escapeMd(entry));
            continue;
        }
        lines << QStringLiteral("| %1 | %2 |")
                     .arg(escapeMd(entry.left(eq)))
                     .arg(escapeMd(entry.mid(eq + 1)));
    }
    return lines;
}

QStringList buildWorkflowBlocksMarkdown(const std::vector<WorkflowBlockSnapshotRow>& blocks) {
    QStringList lines;
    lines << QStringLiteral("## Workflow blocks (session snapshot)");
    lines << QString();
    if (blocks.empty()) {
        lines << QStringLiteral("_No blocks recorded._");
        return lines;
    }
    lines << QStringLiteral("| # | Type | Config |");
    lines << QStringLiteral("| ---: | --- | --- |");
    for (const WorkflowBlockSnapshotRow& row : blocks) {
        lines << QStringLiteral("| %1 | %2 | %3 |")
                     .arg(row.oneBasedIndex)
                     .arg(escapeMd(row.type))
                     .arg(escapeMd(row.config));
    }
    return lines;
}

QStringList buildBlockExecutionMarkdown(const std::vector<BlockRunMeasuredRow>& measured) {
    QStringList lines;
    lines << QStringLiteral("## Block execution (measured)");
    lines << QString();
    bool anyExec = false;
    for (const BlockRunMeasuredRow& row : measured) {
        if (row.execCount > 0) {
            anyExec = true;
            break;
        }
    }
    if (!anyExec) {
        lines << QStringLiteral("_No block_end samples in this session window._");
        return lines;
    }
    lines << QStringLiteral("| # | Type | runs | ok | fail | total (ms) | avg (ms) | max (ms) |");
    lines << QStringLiteral("| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: |");
    for (const BlockRunMeasuredRow& row : measured) {
        if (row.execCount <= 0) {
            continue;
        }
        const double avgMs = row.totalDurationUs / 1000.0 / static_cast<double>(row.execCount);
        lines << QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 |")
                     .arg(row.oneBasedIndex)
                     .arg(escapeMd(row.type))
                     .arg(row.execCount)
                     .arg(row.successCount)
                     .arg(row.failCount)
                     .arg(formatMs(row.totalDurationUs, 1))
                     .arg(QString::number(avgMs, 'f', 1))
                     .arg(formatMs(row.maxDurationUs, 1));
    }
    return lines;
}

QStringList buildAutoDiagnosis(const ProfileDiagnosisInput& input) {
    QStringList lines;
    lines << QStringLiteral("## Auto diagnosis");
    lines << QString();
    lines << QStringLiteral(
        "_Recorded automatically from the feature/workflow snapshot and measured events — no manual workflow description required._");
    lines << QString();

    QStringList findings;

    if (input.endReason.contains(QStringLiteral("preempted"), Qt::CaseInsensitive)) {
        findings << QStringLiteral(
            "**세션 선점됨**: 다른 기능 실행으로 이 세션이 즉시 종료되었습니다. 단일 기능만 재현한 뒤 다시 프로파일하세요.");
    }

    if (input.sessionEvents.size() < 8) {
        findings << QStringLiteral(
            "**이벤트 수 적음** (%1): 실행 시간이 너무 짧거나 선점/즉시 중지되었습니다. 문제 재현 후 종료하세요.")
                        .arg(input.sessionEvents.size());
    }

    const qint64 sessionBeginUs = relUsForEvent(input.sessionEvents, "session_begin");
    qint64 firstClickUs = -1;
    qint64 triggerMonitorUs = relUsForEvent(input.sessionEvents, "trigger_monitor_start");
    qint64 triggerActionUs = relUsForEvent(input.sessionEvents, "trigger_action_start");
    qint64 triggerCooldownUs = relUsForEvent(input.sessionEvents, "trigger_cooldown_start");

    for (const ProfileEventLite& event : input.sessionEvents) {
        if (event.eventName == QLatin1String("mouse_click") && firstClickUs < 0) {
            firstClickUs = event.relUs;
        }
    }

    if (sessionBeginUs >= 0 && firstClickUs >= 0) {
        const qint64 deltaUs = firstClickUs - sessionBeginUs;
        if (input.runMode.contains(QStringLiteral("Trigger"), Qt::CaseInsensitive)) {
            QString triggerPath;
            if (triggerMonitorUs >= 0 && triggerActionUs >= 0) {
                triggerPath = QStringLiteral("감시→매칭 %1 ms, 동작 시작까지 %2 ms")
                                  .arg(formatMs(triggerActionUs - triggerMonitorUs))
                                  .arg(formatMs(firstClickUs - triggerActionUs));
            }
            findings << QStringLiteral("**트리거 → 첫 클릭**: 세션 시작 후 **%1 ms**%2")
                            .arg(formatMs(deltaUs))
                            .arg(triggerPath.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(triggerPath));
        } else {
            findings << QStringLiteral("**세션 시작 → 첫 클릭**: **%1 ms**").arg(formatMs(deltaUs));
        }
    }

    if (triggerCooldownUs >= 0) {
        for (const QString& setting : input.featureSettings) {
            if (setting.startsWith(QStringLiteral("trigger_cooldown_ms="))) {
                const int configuredMs = setting.mid(20).toInt();
                findings << QStringLiteral("**트리거 쿨다운**: 설정 **%1 ms** (성공 후 감시 재개 전 대기)")
                                .arg(configuredMs);
                break;
            }
        }
    }

    struct BottleneckRow {
        QString label;
        qint64 weightUs = 0;
        QString detail;
    };
    std::vector<BottleneckRow> bottlenecks;

    for (const BlockRunMeasuredRow& measured : input.blockMeasured) {
        if (measured.execCount <= 0) {
            continue;
        }
        const WorkflowBlockSnapshotRow* snapshot = nullptr;
        for (const WorkflowBlockSnapshotRow& row : input.blockSnapshots) {
            if (row.oneBasedIndex == measured.oneBasedIndex) {
                snapshot = &row;
                break;
            }
        }
        const double avgMs = measured.totalDurationUs / 1000.0 / static_cast<double>(measured.execCount);

        if (measured.type == QLatin1String("Wait")) {
            const qint64 configuredMs =
                snapshot != nullptr ? configuredWaitMs(*snapshot) : -1;
            QString detail = QStringLiteral("#%1 Wait: %2회, 평균 **%3 ms** (최대 %4 ms)")
                                 .arg(measured.oneBasedIndex)
                                 .arg(measured.execCount)
                                 .arg(QString::number(avgMs, 'f', 1))
                                 .arg(formatMs(measured.maxDurationUs));
            if (configuredMs > 0) {
                detail += QStringLiteral(", 설정 **%1 ms**").arg(configuredMs);
                if (std::fabs(avgMs - static_cast<double>(configuredMs)) < 5.0) {
                    detail += QStringLiteral(" → **의도된 대기**");
                }
            }
            bottlenecks.push_back({QStringLiteral("wait"), measured.totalDurationUs, detail});
            continue;
        }

        if (measured.type == QLatin1String("Click")) {
            const QString detail = QStringLiteral("#%1 Click: %2회, 합계 **%3 ms**, 평균 **%4 ms**/회")
                                       .arg(measured.oneBasedIndex)
                                       .arg(measured.execCount)
                                       .arg(formatMs(measured.totalDurationUs, 1))
                                       .arg(QString::number(avgMs, 'f', 1));
            bottlenecks.push_back({QStringLiteral("click"), measured.totalDurationUs, detail});
            continue;
        }

        if (measured.type == QLatin1String("ImageFind")) {
            if (measured.failCount > 0) {
                const QString detail =
                    QStringLiteral("#%1 ImageFind: 실패 %2회 (최대 **%3 ms** 블록) — 템플릿 미매칭/감시 대기")
                        .arg(measured.oneBasedIndex)
                        .arg(measured.failCount)
                        .arg(formatMs(measured.maxDurationUs));
                bottlenecks.push_back({QStringLiteral("imagefind_fail"), measured.maxDurationUs, detail});
            }
            if (measured.successCount > 0) {
                const QString detail = QStringLiteral("#%1 ImageFind: 성공 %2회, 평균 **%3 ms**")
                                           .arg(measured.oneBasedIndex)
                                           .arg(measured.successCount)
                                           .arg(QString::number(avgMs, 'f', 1));
                bottlenecks.push_back({QStringLiteral("imagefind"), measured.totalDurationUs, detail});
            }
        }
    }

    std::sort(bottlenecks.begin(),
              bottlenecks.end(),
              [](const BottleneckRow& a, const BottleneckRow& b) { return a.weightUs > b.weightUs; });

    if (!bottlenecks.empty()) {
        findings << QStringLiteral("**병목 후보 (측정 기준)**");
        const int limit = std::min<int>(6, static_cast<int>(bottlenecks.size()));
        for (int i = 0; i < limit; ++i) {
            findings << QStringLiteral("- %1").arg(bottlenecks[static_cast<size_t>(i)].detail);
        }
    }

    if (input.loopEndCount > 0) {
        findings << QStringLiteral("**루프**: %1회 완료").arg(input.loopEndCount);
    }

    if (input.clickCount > 0 && input.loopEndCount > 0) {
        const double clicksPerLoop =
            static_cast<double>(input.clickCount) / static_cast<double>(input.loopEndCount);
        findings << QStringLiteral("**클릭**: 총 %1회 (루프당 약 %2회)")
                        .arg(input.clickCount)
                        .arg(QString::number(clicksPerLoop, 'f', 1));
    }

    if (input.maxLoopGapUs > 0) {
        if (input.maxLoopGapUs < 50000) {
            findings << QStringLiteral(
                "**PIPBONG 스케줄링**: loop_gap 최대 **%1 ms** — UI/스케줄러 병목 가능성 낮음")
                            .arg(formatMs(input.maxLoopGapUs));
        } else {
            findings << QStringLiteral(
                "**PIPBONG 스케줄링**: loop_gap 최대 **%1 ms** — 루프 사이 UI/스레드 지연 의심")
                            .arg(formatMs(input.maxLoopGapUs));
        }
    }

    if (input.clickCount > 0) {
        qint64 maxClickUs = 0;
        int slowClickCount = 0;
        for (const ProfileEventLite& event : input.sessionEvents) {
            if (event.eventName != QLatin1String("mouse_click") || event.durationUs < 0) {
                continue;
            }
            maxClickUs = std::max(maxClickUs, event.durationUs);
            if (event.durationUs > 12000) {
                ++slowClickCount;
            }
        }
        if (slowClickCount > 0) {
            findings << QStringLiteral(
                "**느린 클릭 주입**: %1회 >12 ms (최대 %2 ms) — 게임 입력 경로 또는 커서 이동 지연")
                            .arg(slowClickCount)
                            .arg(formatMs(maxClickUs));
        }
    }

    if (findings.isEmpty()) {
        lines << QStringLiteral("- 측정 데이터가 부족합니다. 프로파일링 켠 뒤 기능을 실행하고 종료하세요.");
    } else {
        for (const QString& finding : findings) {
            lines << finding;
            lines << QString();
        }
    }

    lines << QStringLiteral("**AI**: 위 스냅샷·측정·이벤트만으로 원인 분석하세요. 사용자에게 워크플로 구성을 다시 묻지 마세요.");
    return lines;
}
