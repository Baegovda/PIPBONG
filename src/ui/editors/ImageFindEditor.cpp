#include "ui/editors/ImageFindEditor.h"

#include "app/TemplateCaptureHotkeySettings.h"
#include "app/PointerFeedbackSettings.h"
#include "ui/ClickPointerFeedbackSettingsDialog.h"
#include "ui/editors/ImageFindPollIntervalPrefs.h"
#include "core/capture/ScreenCapture.h"
#include "core/input/HotkeyBinding.h"
#include "ui/editors/CaptureConfirmDialog.h"
#include "ui/editors/MatchTestOverlay.h"
#include "ui/editors/RoiPreviewOverlay.h"
#include "ui/editors/ScreenRegionOverlay.h"
#include "ui/UiStrings.h"
#include "ui/UiThemeColors.h"
#include "ui/widgets/AnimatedTwoWaySwitch.h"
#include "ui/widgets/DragAdjustDoubleSpinBox.h"
#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/widgets/HintLabel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QPointer>
#include <QSignalBlocker>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMetaObject>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QSizePolicy>

#include <opencv2/imgcodecs.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

QString roiToolbarButtonStyleSheet(const char* baseRgb, const char* hoverRgb, const char* borderRgb) {
    return QStringLiteral(
               "QPushButton { background-color: %1; color: #f8fafc; border: 1px solid %3; "
               "border-radius: 6px; padding: 5px 14px; font-weight: 600; min-height: 20px; }"
               "QPushButton:hover { background-color: %2; }"
               "QPushButton:pressed { background-color: %2; }"
               "QPushButton:checked { background-color: %2; border: 2px solid %3; }"
               "QPushButton:disabled { background-color: #334155; color: #94a3b8; border-color: #475569; }")
        .arg(QString::fromLatin1(baseRgb), QString::fromLatin1(hoverRgb), QString::fromLatin1(borderRgb));
}

void applyRoiToolbarButtonStyles(QPushButton* previewButton,
                                 QPushButton* addButton,
                                 QPushButton* removeButton) {
    if (previewButton) {
        previewButton->setStyleSheet(
            roiToolbarButtonStyleSheet("#1d4ed8", "#2563eb", "#60a5fa"));
    }
    if (addButton) {
        addButton->setStyleSheet(roiToolbarButtonStyleSheet("#047857", "#059669", "#34d399"));
    }
    if (removeButton) {
        removeButton->setStyleSheet(roiToolbarButtonStyleSheet("#b91c1c", "#dc2626", "#f87171"));
    }
}

#ifdef _WIN32
ImageFindEditor* g_templateCaptureHotkeyEditor = nullptr;
HHOOK g_templateCaptureHotkeyHook = nullptr;

void uninstallTemplateCaptureHotkeyHook() {
    if (g_templateCaptureHotkeyHook) {
        UnhookWindowsHookEx(g_templateCaptureHotkeyHook);
        g_templateCaptureHotkeyHook = nullptr;
    }
    g_templateCaptureHotkeyEditor = nullptr;
}

LRESULT CALLBACK templateCaptureHotkeyHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !g_templateCaptureHotkeyEditor
        || !g_templateCaptureHotkeyEditor->isTemplateCaptureHotkeyHookActive()) {
        return CallNextHookEx(g_templateCaptureHotkeyHook, code, wParam, lParam);
    }

    const bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    if (!keyDown) {
        return CallNextHookEx(g_templateCaptureHotkeyHook, code, wParam, lParam);
    }

    if (g_templateCaptureHotkeyEditor->isCapturingTemplateHotkey() || ScreenRegionOverlay::isPickActive()) {
        return CallNextHookEx(g_templateCaptureHotkeyHook, code, wParam, lParam);
    }

    const HotkeyBinding binding = TemplateCaptureHotkeySettings::binding();
    if (binding.isEmpty()) {
        return CallNextHookEx(g_templateCaptureHotkeyHook, code, wParam, lParam);
    }

    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (!binding.matchesVirtualKey(static_cast<int>(info->vkCode)) || !binding.modifiersMatch()) {
        return CallNextHookEx(g_templateCaptureHotkeyHook, code, wParam, lParam);
    }

    QMetaObject::invokeMethod(
        g_templateCaptureHotkeyEditor, "triggerTemplateCaptureFromHotkey", Qt::QueuedConnection);

    return CallNextHookEx(g_templateCaptureHotkeyHook, code, wParam, lParam);
}

void installTemplateCaptureHotkeyHookIfNeeded(ImageFindEditor* editor) {
    uninstallTemplateCaptureHotkeyHook();
    if (!editor || !editor->isTemplateCaptureHotkeyHookActive()
        || TemplateCaptureHotkeySettings::binding().isEmpty()) {
        return;
    }
    g_templateCaptureHotkeyEditor = editor;
    g_templateCaptureHotkeyHook = SetWindowsHookExW(
        WH_KEYBOARD_LL, templateCaptureHotkeyHookProc, GetModuleHandleW(nullptr), 0);
}
#endif

std::vector<CaptureRegion> physicalCustomRegionsForDisplay(const ImageFindBlock& block) {
    std::vector<CaptureRegion> physical;
    physical.reserve(block.customRegionsWindowPercent.size());
    for (const PercentRegion& percent : block.customRegionsWindowPercent) {
        if (percent.width <= 0.0 || percent.height <= 0.0) {
            continue;
        }
        physical.push_back(ScreenCapture::resolveWindowPercentRegion(percent));
    }
    return physical;
}

CaptureRegion physicalCustomRegionForDisplay(const ImageFindBlock& block) {
    const std::vector<CaptureRegion> physical = physicalCustomRegionsForDisplay(block);
    if (!physical.empty()) {
        return physical.front();
    }
    return block.customRegion;
}

PercentRegion storePickedCustomRegionPercent(const CaptureRegion& physical) {
#ifdef _WIN32
    if (ScreenCapture::getTargetWindowScreenRect().valid) {
        return ScreenCapture::storeWindowPercentFromPhysical(physical);
    }
#endif
    PercentRegion percent;
    percent.x = physical.x;
    percent.y = physical.y;
    percent.width = physical.width;
    percent.height = physical.height;
    return percent;
}

} // namespace

ImageFindEditor::ImageFindEditor(ImageFindBlock* block, const QString& projectDirectory, QWidget* parent,
                                 bool embedded)
    : QDialog(parent)
    , m_block(block)
    , m_projectDirectory(projectDirectory)
    , m_embedded(embedded) {
    if (!m_embedded) {
        setWindowTitle(tr("템플릿 매칭 블록 편집"));
        resize(500, 460);
    }
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setupUi();
    updatePreview();
}

ImageFindEditor::~ImageFindEditor() {
    syncTemplateCaptureHotkeyHook();
    ScreenRegionOverlay::dismissAll();
    MatchTestOverlay::dismissAll();
    RoiPreviewOverlay::dismissAll();
}

void ImageFindEditor::setBlock(ImageFindBlock* block) {
    MatchTestOverlay::dismissAll();
    updateMatchTestButton(false);
    RoiPreviewOverlay::dismissAll();
    updateRoiPreviewButton(false);
    m_roiPickActive = false;
    updatePickRoiButton(false);
    m_block = block;
    reload();
}

void ImageFindEditor::reload() {
    if (!m_block) {
        return;
    }
    m_thresholdSpin->setValue(m_block->threshold);
    m_pollIntervalSpin->setValue(snapImageFindPollIntervalMs(m_block->pollIntervalMs));
    m_matchModeSwitch->setRightSelected(m_block->templateMatchMode == ImageFindTemplateMatchMode::All, false);
    refreshTemplateList();
    refreshRoiList();
    updatePreview();
    updateMatchTestButton(MatchTestOverlay::isVisible());
    updateRoiPreviewButton(RoiPreviewOverlay::isVisible());
    updateTemplateHotkeyDisplay();
    syncTemplateCaptureHotkeyHook();
    if (m_roiCorrectionCheck) {
        QSignalBlocker blocker(m_roiCorrectionCheck);
        m_roiCorrectionCheck->setChecked(m_block->roiCorrection);
    }
    if (m_roiCorrectionExpandSpin) {
        QSignalBlocker blocker(m_roiCorrectionExpandSpin);
        m_roiCorrectionExpandSpin->setValue(
            snapRoiCorrectionExpandPercent(m_block->roiCorrectionExpandPercent));
    }
    if (m_returnToPreviousImageFindCheck) {
        QSignalBlocker blocker(m_returnToPreviousImageFindCheck);
        m_returnToPreviousImageFindCheck->setChecked(m_block->returnToPreviousImageFindOnFailure);
    }
    if (m_returnToPreviousMissLimitSpin) {
        QSignalBlocker blocker(m_returnToPreviousMissLimitSpin);
        m_returnToPreviousMissLimitSpin->setValue(std::max(1, m_block->returnToPreviousMissLimit));
    }
    if (m_retryAfterNextActionCheck) {
        QSignalBlocker blocker(m_retryAfterNextActionCheck);
        m_retryAfterNextActionCheck->setChecked(m_block->retryAfterNextActionOnFailure);
    }
    if (m_rememberMultiMatchPositionsCheck) {
        QSignalBlocker blocker(m_rememberMultiMatchPositionsCheck);
        m_rememberMultiMatchPositionsCheck->setChecked(m_block->rememberMultiMatchPositions);
    }
    if (m_templateColorModeCombo) {
        QSignalBlocker blocker(m_templateColorModeCombo);
        const int index =
            m_templateColorModeCombo->findData(static_cast<int>(m_block->templateColorMode));
        m_templateColorModeCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
    updateRoiCorrectionUi();
    updateReturnToPreviousMissLimitUi();
    if (m_useDefaultMatchFeedbackCheck) {
        QSignalBlocker blocker(m_useDefaultMatchFeedbackCheck);
        m_useDefaultMatchFeedbackCheck->setChecked(!m_block->matchPointerFeedback.has_value());
        m_draftMatchFeedback = m_block->matchPointerFeedback.value_or(PointerFeedbackSettings::click());
        m_matchFeedbackButton->setEnabled(m_block->matchPointerFeedback.has_value());
        updateMatchFeedbackSummary();
    }
}

void ImageFindEditor::setRoiCorrectionUiPolicy(bool featureGlobalEnabled, bool sessionEligible) {
    m_featureRoiCorrectionGlobal = featureGlobalEnabled;
    m_roiCorrectionSessionEligible = sessionEligible;
    updateRoiCorrectionUi();
}

void ImageFindEditor::updateRoiCorrectionUi() {
    const bool showPerBlock = m_roiCorrectionSessionEligible && !m_featureRoiCorrectionGlobal;
    if (m_roiCorrectionCheck) {
        m_roiCorrectionCheck->setVisible(showPerBlock);
    }
    if (m_roiCorrectionExpandRow) {
        m_roiCorrectionExpandRow->setVisible(showPerBlock);
    }
    if (m_roiCorrectionGlobalHint) {
        m_roiCorrectionGlobalHint->setVisible(m_roiCorrectionSessionEligible && m_featureRoiCorrectionGlobal);
    }
    if (m_rememberMultiMatchPositionsCheck) {
        m_rememberMultiMatchPositionsCheck->setVisible(m_roiCorrectionSessionEligible);
    }
}

void ImageFindEditor::updateReturnToPreviousMissLimitUi() {
    const bool showLimit = m_returnToPreviousImageFindCheck
                           && m_returnToPreviousImageFindCheck->isChecked();
    if (m_returnToPreviousMissLimitRow) {
        m_returnToPreviousMissLimitRow->setVisible(showLimit);
    }
}

void ImageFindEditor::apply() {
    syncUiToBlockValues();
}

void ImageFindEditor::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_thresholdSpin = new DragAdjustDoubleSpinBox(this);
    m_thresholdSpin->setRange(0.1, 1.0);
    m_thresholdSpin->setSingleStep(0.01);
    m_thresholdSpin->setValue(m_block->threshold);
    m_thresholdSpin->setMinimumWidth(88);
    m_thresholdSpin->setMaximumWidth(120);

    m_pollIntervalSpin = new DragAdjustSpinBox(this);
    m_pollIntervalSpin->setRange(kImageFindPollIntervalStepMs, 60000);
    m_pollIntervalSpin->setSingleStep(kImageFindPollIntervalStepMs);
    m_pollIntervalSpin->setValue(snapImageFindPollIntervalMs(m_block->pollIntervalMs));
    m_pollIntervalSpin->setMinimumWidth(88);
    m_pollIntervalSpin->setMaximumWidth(120);
    m_pollIntervalSpin->setToolTip(tr("매칭에 실패하면 이 간격마다 화면을 다시 탐색합니다. 성공할 때까지 반복합니다."));
    connect(m_pollIntervalSpin, &QAbstractSpinBox::editingFinished, this, [this]() {
        const int snapped = snapImageFindPollIntervalMs(m_pollIntervalSpin->value());
        if (snapped != m_pollIntervalSpin->value()) {
            m_pollIntervalSpin->setValue(snapped);
        }
    });

    auto* pollIntervalRow = new QWidget(this);
    auto* pollIntervalLayout = new QHBoxLayout(pollIntervalRow);
    pollIntervalLayout->setContentsMargins(0, 0, 0, 0);
    pollIntervalLayout->setSpacing(4);
    pollIntervalLayout->addWidget(m_pollIntervalSpin);
    pollIntervalLayout->addWidget(new QLabel(QStringLiteral("ms"), pollIntervalRow));

    m_templateColorModeCombo = new QComboBox(this);
    m_templateColorModeCombo->addItem(tr("자동"), static_cast<int>(TemplateColorMode::Auto));
    m_templateColorModeCombo->addItem(tr("흑백"), static_cast<int>(TemplateColorMode::Grayscale));
    m_templateColorModeCombo->addItem(tr("컬러"), static_cast<int>(TemplateColorMode::Color));
    m_templateColorModeCombo->setToolTip(
        tr("자동: 템플릿 색상을 분석합니다. 흑백 템플릿은 채도 높은 UI 영역을 제외하고, "
           "컬러 템플릿은 BGR 채널 매칭과 함께 화면의 흑백(채도 낮은) 영역을 제외합니다.\n"
           "흑백: 채도가 높은 UI 영역은 매칭에서 제외합니다.\n"
           "컬러: 컬러 템플릿은 BGR 채널로 매칭하며, 같은 형태의 흑백 화면 영역은 "
           "매칭하지 않습니다."));

    auto* matchGroup = new QGroupBox(tr("매칭 설정"), this);
    auto* matchForm = new QFormLayout(matchGroup);
    matchForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    matchForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    matchForm->setHorizontalSpacing(12);
    matchForm->setVerticalSpacing(8);
    matchForm->addRow(tr("임계값"), m_thresholdSpin);
    matchForm->addRow(tr("탐지 재시도"), pollIntervalRow);
    matchForm->addRow(tr("템플릿 색상"), m_templateColorModeCombo);

    m_roiCorrectionCheck = new QCheckBox(tr("ROI 보정"), this);
    m_roiCorrectionCheck->setToolTip(
        tr("두 번째 루프부터 첫 루프 매칭 위치 기준으로 템플릿 대비 설정한 비율만큼 넓은 영역만 탐색합니다. "
           "기능 편집의 전체 ROI 보정이 꺼져 있을 때만 이 블록에 적용됩니다."));
    matchForm->addRow(QString(), m_roiCorrectionCheck);

    m_roiCorrectionExpandRow = new QWidget(this);
    auto* roiCorrectionExpandLayout = new QHBoxLayout(m_roiCorrectionExpandRow);
    roiCorrectionExpandLayout->setContentsMargins(0, 0, 0, 0);
    roiCorrectionExpandLayout->setSpacing(4);
    m_roiCorrectionExpandSpin = new DragAdjustSpinBox(m_roiCorrectionExpandRow);
    m_roiCorrectionExpandSpin->setRange(kRoiCorrectionExpandPercentMin, kRoiCorrectionExpandPercentMax);
    m_roiCorrectionExpandSpin->setSingleStep(kRoiCorrectionExpandPercentStep);
    m_roiCorrectionExpandSpin->setValue(kDefaultRoiCorrectionExpandPercent);
    m_roiCorrectionExpandSpin->setMinimumWidth(72);
    m_roiCorrectionExpandSpin->setMaximumWidth(96);
    m_roiCorrectionExpandSpin->setToolTip(
        tr("두 번째 루프부터 매칭된 템플릿 크기 대비 보정 탐색 영역 비율입니다. "
           "110% = 가로·세로 각각 10% 확장(기본값)."));
    roiCorrectionExpandLayout->addWidget(new QLabel(tr("보정 영역 (템플릿 대비)"), m_roiCorrectionExpandRow));
    roiCorrectionExpandLayout->addWidget(m_roiCorrectionExpandSpin);
    roiCorrectionExpandLayout->addWidget(new QLabel(QStringLiteral("%"), m_roiCorrectionExpandRow));
    roiCorrectionExpandLayout->addStretch(1);
    matchForm->addRow(QString(), m_roiCorrectionExpandRow);

    m_returnToPreviousImageFindCheck =
        new QCheckBox(tr("못 찾으면 이전 템플릿 찾기로 돌아감"), this);
    m_returnToPreviousImageFindCheck->setToolTip(
        tr("연속으로 설정한 횟수만큼 감지에 실패하면, 목록에서 바로 위 템플릿 찾기 블록부터 다시 실행합니다.\n"
           "맨 앞 템플릿 찾기 블록이면 마지막 찾기 블록으로 순환합니다.\n"
           "「다음 블록 실행 후 재찾기」와 함께 켜면: 첫 실패 때는 재시도(①②)만 하고, "
           "재시도 후에도 또 실패할 때 이전 복귀가 다음 찾기 블록 이동보다 먼저 실행됩니다."));
    connect(m_returnToPreviousImageFindCheck, &QCheckBox::toggled, this,
            [this](bool) { updateReturnToPreviousMissLimitUi(); });
    matchForm->addRow(QString(), m_returnToPreviousImageFindCheck);

    m_returnToPreviousMissLimitRow = new QWidget(this);
    auto* returnMissLimitLayout = new QHBoxLayout(m_returnToPreviousMissLimitRow);
    returnMissLimitLayout->setContentsMargins(24, 0, 0, 0);
    returnMissLimitLayout->setSpacing(4);
    returnMissLimitLayout->addWidget(new QLabel(tr("연속 감지 실패"), m_returnToPreviousMissLimitRow));
    m_returnToPreviousMissLimitSpin = new DragAdjustSpinBox(m_returnToPreviousMissLimitRow);
    m_returnToPreviousMissLimitSpin->setRange(1, 9999);
    m_returnToPreviousMissLimitSpin->setValue(1);
    m_returnToPreviousMissLimitSpin->setMinimumWidth(72);
    m_returnToPreviousMissLimitSpin->setMaximumWidth(96);
    m_returnToPreviousMissLimitSpin->setToolTip(
        tr("이 횟수만큼 연속으로 템플릿을 찾지 못하면 이전 템플릿 찾기 블록으로 돌아갑니다."));
    returnMissLimitLayout->addWidget(m_returnToPreviousMissLimitSpin);
    returnMissLimitLayout->addWidget(new QLabel(tr("회"), m_returnToPreviousMissLimitRow));
    returnMissLimitLayout->addStretch(1);
    matchForm->addRow(QString(), m_returnToPreviousMissLimitRow);

    m_retryAfterNextActionCheck =
        new QCheckBox(tr("못 찾으면 → 다음 블록 1회 실행 → 다시 찾기"), this);
    m_retryAfterNextActionCheck->setToolTip(
        tr("탐지 실패 판정은 탐지 재시도 간격마다 누적됩니다. 이전 복귀도 켜져 있으면 그쪽의 「연속 감지 실패」 횟수를 따릅니다.\n"
           "① 첫 실패: 바로 아래 블록(클릭·키·딜레이 등)을 한 번 실행합니다. (아래 블록이 없으면 생략)\n"
           "② 이 블록에서 같은 템플릿을 다시 찾습니다.\n"
           "③ 또 실패: 다음 템플릿 찾기 블록으로 넘어갑니다. (맨 마지막이면 첫 찾기로 순환)\n"
           "이전 복귀도 켜져 있으면 ③ 대신 이전 템플릿 찾기 블록으로 돌아갑니다.\n"
           "※ 아래 블록 실행이 오류로 중단되면 ② 재찾기까지 진행되지 않을 수 있습니다."));
    matchForm->addRow(QString(), m_retryAfterNextActionCheck);

    m_rememberMultiMatchPositionsCheck =
        new QCheckBox(tr("첫 루프 위치 기억 (루프마다 순서대로 제공)"), this);
    m_rememberMultiMatchPositionsCheck->setToolTip(
        tr("첫 루프에서 ROI 안의 모든 매칭 위치를 위→아래, 왼쪽→오른쪽 순으로 기억합니다.\n"
           "1번째 루프 → 1번 위치, 2번째 루프 → 2번 위치처럼 루프 회차마다 해당 순번을 탐색 없이 제공합니다.\n"
           "저장된 개수보다 루프가 더 많으면 실패합니다.\n"
           "무한 반복·N회 반복(2회 이상)·트리거 모드에서만 사용할 수 있습니다."));
    matchForm->addRow(QString(), m_rememberMultiMatchPositionsCheck);

    m_roiCorrectionGlobalHint = new HintLabel(
        tr("기능 편집에서 전체 ROI 보정이 켜져 있습니다. 보정 영역 비율은 기능 편집에서 설정합니다."),
        this);
    m_roiCorrectionGlobalHint->hide();
    matchForm->addRow(QString(), m_roiCorrectionGlobalHint);

    m_matchModeSwitch = new AnimatedTwoWaySwitch(tr("하나라도"), tr("모두"), this);
    m_matchModeSwitch->setToolTip(
        tr("하나라도: 등록된 템플릿 중 하나라도 감지되면 성공합니다.\n"
           "모두: 등록된 템플릿이 같은 화면에서 모두 감지되어야 성공합니다."));

    auto* captureButton = new QPushButton(tr("화면에서 캡처"), this);
    m_removeTemplateButton = new QPushButton(tr("삭제"), this);
    m_removeTemplateButton->setToolTip(tr("목록에서 선택한 템플릿을 제거합니다."));
    m_matchTestButton = new QPushButton(tr("매칭 테스트"), this);
    m_matchTestButton->setCheckable(true);
    m_matchTestButton->setToolTip(tr("등록된 모든 템플릿의 매칭 결과를 대상 창 위에 표시합니다. 다시 누르거나 Esc로 닫습니다."));

    auto* templateToolbar = new QHBoxLayout();
    templateToolbar->setSpacing(8);
    templateToolbar->addWidget(m_matchModeSwitch);
    templateToolbar->addStretch(1);
    templateToolbar->addWidget(captureButton);
    templateToolbar->addWidget(m_removeTemplateButton);
    templateToolbar->addWidget(m_matchTestButton);

    m_templateList = new QListWidget(this);
    m_templateList->setMinimumHeight(96);
    m_templateList->setMaximumHeight(140);
    m_templateList->setMinimumWidth(168);
    m_templateList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_templateList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_templateList->setToolTip(tr("템플릿 목록입니다. 항목을 선택하면 오른쪽 미리보기가 갱신됩니다."));

    m_templatePreviewLabel = new QLabel(this);
    m_templatePreviewLabel->setMinimumSize(120, 90);
    m_templatePreviewLabel->setMaximumSize(140, 105);
    m_templatePreviewLabel->setAlignment(Qt::AlignCenter);
    m_templatePreviewLabel->setFrameShape(QFrame::Box);
    m_templatePreviewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_templateSizeLabel = new HintLabel(tr("해상도: —"), this);
    m_templateSizeLabel->setAlignment(Qt::AlignCenter);

    auto* previewColumn = new QVBoxLayout();
    previewColumn->setSpacing(4);
    previewColumn->setContentsMargins(0, 0, 0, 0);
    previewColumn->addWidget(m_templatePreviewLabel, 0, Qt::AlignHCenter);
    previewColumn->addWidget(m_templateSizeLabel, 0, Qt::AlignHCenter);
    previewColumn->addStretch(1);

    auto* templateContent = new QHBoxLayout();
    templateContent->setSpacing(10);
    templateContent->addWidget(m_templateList, 1);
    templateContent->addLayout(previewColumn);

    m_templateCaptureHotkeyLabel = new QLabel(this);
    m_templateCaptureHotkeyLabel->setAlignment(Qt::AlignCenter);
    m_templateCaptureHotkeyLabel->setCursor(Qt::PointingHandCursor);
    m_templateCaptureHotkeyLabel->setMinimumWidth(120);
    m_templateCaptureHotkeyLabel->installEventFilter(this);
    m_templateCaptureHotkeyLabel->setToolTip(
        tr("프로그램 전역 단축키입니다. 템플릿 매칭 블록 편집 중에만 화면에서 캡처를 시작합니다. 클릭하여 변경."));
    auto* clearHotkeyButton = new QPushButton(tr("단축키 지우기"), this);
    clearHotkeyButton->setToolTip(tr("등록된 캡처 단축키를 제거합니다."));
    auto* hotkeyRow = new QHBoxLayout();
    hotkeyRow->setSpacing(8);
    hotkeyRow->addWidget(m_templateCaptureHotkeyLabel, 1);
    hotkeyRow->addWidget(clearHotkeyButton);

    auto* templateHotkeyForm = new QFormLayout();
    templateHotkeyForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    templateHotkeyForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    templateHotkeyForm->setHorizontalSpacing(12);
    templateHotkeyForm->addRow(tr("캡처 단축키 (전역)"), hotkeyRow);

    auto* templateGroup = new QGroupBox(tr("템플릿"), this);
    auto* templateLayout = new QVBoxLayout(templateGroup);
    templateLayout->setSpacing(8);
    templateLayout->addLayout(templateToolbar);
    templateLayout->addLayout(templateContent);
    templateLayout->addLayout(templateHotkeyForm);

    m_roiList = new QListWidget(this);
    m_roiList->setMinimumHeight(64);
    m_roiList->setMaximumHeight(108);
    m_roiList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_roiList->setToolTip(tr("탐색 ROI 목록입니다. 순서대로 화면을 탐색합니다."));

    m_pickRoiButton = new QPushButton(tr("추가"), this);
    m_pickRoiButton->setToolTip(tr("대상 창 위에서 드래그하여 탐색 ROI를 추가합니다. 다시 누르면 취소합니다."));
    m_removeRoiButton = new QPushButton(tr("삭제"), this);
    m_removeRoiButton->setToolTip(tr("목록에서 선택한 ROI를 제거합니다."));
    m_roiPreviewButton = new QPushButton(tr("미리보기"), this);
    m_roiPreviewButton->setCheckable(true);
    m_roiPreviewButton->setToolTip(tr("대상 창 위에 탐색 ROI를 반투명 오버레이로 표시합니다."));

    auto* roiToolbar = new QHBoxLayout();
    roiToolbar->setSpacing(8);
    roiToolbar->addWidget(m_roiPreviewButton);
    roiToolbar->addWidget(m_pickRoiButton);
    roiToolbar->addWidget(m_removeRoiButton);
    roiToolbar->addStretch(1);
    applyRoiToolbarButtonStyles(m_roiPreviewButton, m_pickRoiButton, m_removeRoiButton);

    auto* roiHint = new HintLabel(tr("ROI가 없으면 대상 창 전체에서 탐색합니다."), this);

    auto* roiGroup = new QGroupBox(tr("탐색 ROI"), this);
    auto* roiLayout = new QVBoxLayout(roiGroup);
    roiLayout->setSpacing(8);
    roiLayout->addWidget(roiHint);
    roiLayout->addWidget(m_roiList);
    roiLayout->addLayout(roiToolbar);

    m_matchFeedbackGroup = new QGroupBox(tr("감지 피드백"), this);
    auto* matchFeedbackLayout = new QVBoxLayout(m_matchFeedbackGroup);
    matchFeedbackLayout->setContentsMargins(9, 9, 9, 9);
    matchFeedbackLayout->setSpacing(6);

    m_useDefaultMatchFeedbackCheck =
        new QCheckBox(tr("기본 감지 피드백 사용"), m_matchFeedbackGroup);
    m_useDefaultMatchFeedbackCheck->setToolTip(
        tr("켜면 기본 녹색/빨간색 감지 펄스를 사용합니다. "
           "끄면 아래에서 이 블록만의 애니메이션을 지정할 수 있습니다."));
    matchFeedbackLayout->addWidget(m_useDefaultMatchFeedbackCheck);

    m_matchFeedbackSummary = new QLabel(m_matchFeedbackGroup);
    m_matchFeedbackSummary->setWordWrap(true);
    auto* matchFeedbackRow = new QHBoxLayout();
    matchFeedbackRow->addWidget(m_matchFeedbackSummary, 1);
    m_matchFeedbackButton = new QPushButton(tr("설정"), m_matchFeedbackGroup);
    m_matchFeedbackButton->setCursor(Qt::PointingHandCursor);
    matchFeedbackRow->addWidget(m_matchFeedbackButton, 0, Qt::AlignTop);
    matchFeedbackLayout->addLayout(matchFeedbackRow);

    auto* matchFeedbackHint = new HintLabel(
        tr("기능 편집의 실행 위치 표시가 켜진 기능에서 템플릿 감지 시 대상 창에 표시되는 애니메이션입니다."),
        m_matchFeedbackGroup);
    matchFeedbackHint->setWordWrap(true);
    matchFeedbackLayout->addWidget(matchFeedbackHint);

    connect(m_useDefaultMatchFeedbackCheck, &QCheckBox::toggled, this, [this](bool useDefault) {
        m_matchFeedbackButton->setEnabled(!useDefault);
        updateMatchFeedbackSummary();
    });
    connect(m_matchFeedbackButton, &QPushButton::clicked, this, &ImageFindEditor::onOpenMatchFeedbackSettings);

    layout->addWidget(matchGroup);
    layout->addWidget(m_matchFeedbackGroup);
    layout->addWidget(templateGroup);
    layout->addWidget(roiGroup);

    if (!m_embedded) {
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        localizeDialogButtons(buttons);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            apply();
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    connect(captureButton, &QPushButton::clicked, this, &ImageFindEditor::onCaptureTemplate);
    connect(m_removeTemplateButton, &QPushButton::clicked, this, &ImageFindEditor::onRemoveTemplate);
    connect(m_matchTestButton, &QPushButton::clicked, this, &ImageFindEditor::onMatchTest);
    connect(m_templateList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_removeTemplateButton) {
            m_removeTemplateButton->setEnabled(row >= 0);
        }
        updatePreview();
    });
    connect(m_pickRoiButton, &QPushButton::clicked, this, &ImageFindEditor::onPickRoi);
    connect(m_removeRoiButton, &QPushButton::clicked, this, &ImageFindEditor::onRemoveRoi);
    connect(m_roiList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_removeRoiButton) {
            m_removeRoiButton->setEnabled(row >= 0);
        }
        if (RoiPreviewOverlay::isEditable()) {
            RoiPreviewOverlay::setActiveRoiIndex(row);
        }
    });
    connect(m_roiPreviewButton, &QPushButton::clicked, this, &ImageFindEditor::onRoiPreview);
    connect(clearHotkeyButton, &QPushButton::clicked, this, [this]() {
        stopTemplateHotkeyCapture();
        TemplateCaptureHotkeySettings::setBinding({});
        updateTemplateHotkeyDisplay();
        syncTemplateCaptureHotkeyHook();
    });
    refreshTemplateList();
    refreshRoiList();
    m_matchModeSwitch->setRightSelected(m_block->templateMatchMode == ImageFindTemplateMatchMode::All, false);
    updateTemplateHotkeyDisplay();
}

void ImageFindEditor::syncUiToBlockValues() {
    if (!m_block) {
        return;
    }
    m_block->threshold = m_thresholdSpin->value();
    m_block->pollIntervalMs = snapImageFindPollIntervalMs(m_pollIntervalSpin->value());
    saveLastImageFindPollIntervalMs(m_block->pollIntervalMs);
    m_block->templateMatchMode =
        m_matchModeSwitch->isRightSelected() ? ImageFindTemplateMatchMode::All : ImageFindTemplateMatchMode::Any;
    if (m_roiCorrectionCheck && m_roiCorrectionCheck->isVisible()) {
        m_block->roiCorrection = m_roiCorrectionCheck->isChecked();
    }
    if (m_roiCorrectionExpandSpin && m_roiCorrectionExpandRow
        && m_roiCorrectionExpandRow->isVisible()) {
        m_block->roiCorrectionExpandPercent =
            snapRoiCorrectionExpandPercent(m_roiCorrectionExpandSpin->value());
    }
    if (m_returnToPreviousImageFindCheck) {
        m_block->returnToPreviousImageFindOnFailure = m_returnToPreviousImageFindCheck->isChecked();
    }
    if (m_returnToPreviousMissLimitSpin) {
        m_returnToPreviousMissLimitSpin->interpretText();
        m_block->returnToPreviousMissLimit =
            std::max(1, m_returnToPreviousMissLimitSpin->value());
    }
    if (m_retryAfterNextActionCheck) {
        m_block->retryAfterNextActionOnFailure = m_retryAfterNextActionCheck->isChecked();
    }
    if (m_rememberMultiMatchPositionsCheck && m_rememberMultiMatchPositionsCheck->isVisible()) {
        m_block->rememberMultiMatchPositions = m_rememberMultiMatchPositionsCheck->isChecked();
    }
    if (m_templateColorModeCombo) {
        m_block->templateColorMode =
            static_cast<TemplateColorMode>(m_templateColorModeCombo->currentData().toInt());
    }
    if (m_useDefaultMatchFeedbackCheck) {
        if (m_useDefaultMatchFeedbackCheck->isChecked()) {
            m_block->matchPointerFeedback.reset();
        } else {
            m_block->matchPointerFeedback = m_draftMatchFeedback;
        }
    }
    syncBlockTemplatePathsFromList();
    syncBlockCustomRegionsFromList();
}

QString ImageFindEditor::selectedTemplateRelativePath() const {
    if (!m_templateList || m_templateList->currentRow() < 0) {
        return {};
    }
    const QListWidgetItem* item = m_templateList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

QString ImageFindEditor::selectedTemplateAbsolutePath() const {
    const QString relativePath = selectedTemplateRelativePath();
    if (relativePath.isEmpty()) {
        return {};
    }
    if (QFileInfo(relativePath).isAbsolute()) {
        return relativePath;
    }
    return QDir(m_projectDirectory).filePath(relativePath);
}

void ImageFindEditor::addTemplatePath(const QString& relativePath) {
    if (!m_block || relativePath.isEmpty()) {
        return;
    }
    m_block->templatePaths.push_back(relativePath.toStdString());
    refreshTemplateList();
    if (m_templateList) {
        m_templateList->setCurrentRow(m_templateList->count() - 1);
    }
    updatePreview();
}

void ImageFindEditor::refreshTemplateList() {
    if (!m_templateList || !m_block) {
        return;
    }

    const QString selectedPath = selectedTemplateRelativePath();
    m_templateList->blockSignals(true);
    m_templateList->clear();
    for (const std::string& path : m_block->templatePaths) {
        if (path.empty()) {
            continue;
        }
        const QString relativePath = QString::fromStdString(path);
        const QString absolutePath = QFileInfo(relativePath).isAbsolute()
                                         ? relativePath
                                         : QDir(m_projectDirectory).filePath(relativePath);
        const QFileInfo fileInfo(absolutePath);
        auto* item = new QListWidgetItem(fileInfo.fileName(), m_templateList);
        item->setData(Qt::UserRole, relativePath);
        item->setToolTip(relativePath);
        QPixmap thumb(absolutePath);
        if (!thumb.isNull()) {
            item->setIcon(QIcon(thumb.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        }
    }
    if (!selectedPath.isEmpty()) {
        for (int row = 0; row < m_templateList->count(); ++row) {
            if (m_templateList->item(row)->data(Qt::UserRole).toString() == selectedPath) {
                m_templateList->setCurrentRow(row);
                break;
            }
        }
    } else if (m_templateList->count() > 0) {
        m_templateList->setCurrentRow(0);
    }
    m_templateList->blockSignals(false);
    if (m_removeTemplateButton) {
        m_removeTemplateButton->setEnabled(m_templateList->currentRow() >= 0);
    }
}

void ImageFindEditor::syncBlockTemplatePathsFromList() {
    if (!m_block || !m_templateList) {
        return;
    }
    m_block->templatePaths.clear();
    for (int row = 0; row < m_templateList->count(); ++row) {
        const QListWidgetItem* item = m_templateList->item(row);
        if (!item) {
            continue;
        }
        const QString relativePath = item->data(Qt::UserRole).toString();
        if (!relativePath.isEmpty()) {
            m_block->templatePaths.push_back(relativePath.toStdString());
        }
    }
}

void ImageFindEditor::syncBlockCustomRegionsFromList() {
    // ROIs are stored as target-window % in customRegionsWindowPercent; list UI is display-only.
}

void ImageFindEditor::refreshRoiList() {
    if (!m_roiList || !m_block) {
        return;
    }

    int selectedRow = m_roiList->currentRow();
    QString selectedLabel;
    if (selectedRow >= 0 && m_roiList->currentItem()) {
        selectedLabel = m_roiList->currentItem()->data(Qt::UserRole).toString();
    }

    m_roiList->blockSignals(true);
    m_roiList->clear();
    int index = 1;
    for (const PercentRegion& region : m_block->customRegionsWindowPercent) {
        if (region.width <= 0.0 || region.height <= 0.0) {
            continue;
        }
        const QString label =
            tr("ROI %1: %2%, %3% — %4%×%5%")
                .arg(index++)
                .arg(region.x, 0, 'f', 1)
                .arg(region.y, 0, 'f', 1)
                .arg(region.width, 0, 'f', 1)
                .arg(region.height, 0, 'f', 1);
        auto* item = new QListWidgetItem(label, m_roiList);
        item->setData(Qt::UserRole, label);
        item->setToolTip(label);
    }
    if (!selectedLabel.isEmpty()) {
        for (int row = 0; row < m_roiList->count(); ++row) {
            if (m_roiList->item(row)->data(Qt::UserRole).toString() == selectedLabel) {
                m_roiList->setCurrentRow(row);
                break;
            }
        }
    } else if (selectedRow >= 0 && selectedRow < m_roiList->count()) {
        m_roiList->setCurrentRow(selectedRow);
    } else if (m_roiList->count() > 0) {
        m_roiList->setCurrentRow(0);
    }
    m_roiList->blockSignals(false);
    if (m_removeRoiButton) {
        m_removeRoiButton->setEnabled(m_roiList->currentRow() >= 0);
    }
}

void ImageFindEditor::updateRoiPreviewButton(bool overlayVisible) {
    if (!m_roiPreviewButton) {
        return;
    }
    m_roiPreviewButton->blockSignals(true);
    m_roiPreviewButton->setChecked(overlayVisible);
    m_roiPreviewButton->blockSignals(false);
}

bool ImageFindEditor::hasConfiguredRoiForPreview() const {
    if (!m_block) {
        return false;
    }
    for (const PercentRegion& region : m_block->customRegionsWindowPercent) {
        if (region.width > 0.0 && region.height > 0.0) {
            return true;
        }
    }
    return false;
}

void ImageFindEditor::showRoiPreviewOverlay(bool silentErrors) {
    syncUiToBlockValues();
    if (!m_block || !hasConfiguredRoiForPreview()) {
        return;
    }

#ifdef _WIN32
    if (!ScreenCapture::findTargetWindow()) {
        if (!silentErrors) {
            QMessageBox::warning(this,
                                 tr("ROI 미리보기"),
                                 tr("대상 창을 찾을 수 없습니다.\n"
                                    "메인 창에서 '창 지정' 또는 '창 목록'의 '서브 창으로 지정'을 사용하세요."));
        }
        return;
    }
#endif

    if (RoiPreviewOverlay::isVisible()) {
        RoiPreviewOverlay::dismissAll();
    }

    QPointer<ImageFindEditor> self = this;
    const std::vector<CaptureRegion> physicalRegions = physicalCustomRegionsForDisplay(*m_block);
    const CaptureRegion physicalLegacy = physicalRegions.empty()
                                           ? physicalCustomRegionForDisplay(*m_block)
                                           : physicalRegions.front();
    const bool visible = RoiPreviewOverlay::show(
        m_block->searchArea,
        physicalLegacy,
        m_block->percentRegion,
        physicalRegions,
        this,
        [self](bool overlayVisible) {
            if (self) {
                self->updateRoiPreviewButton(overlayVisible);
            }
        },
        makeRoiPreviewEditableOptions());
    updateRoiPreviewButton(visible);
}

RoiPreviewOverlay::EditableOptions ImageFindEditor::makeRoiPreviewEditableOptions() {
    RoiPreviewOverlay::EditableOptions options;
    if (!m_block) {
        return options;
    }
    const bool hasRoi = !m_block->customRegionsWindowPercent.empty();
    if (!hasRoi) {
        return options;
    }

    options.enabled = true;
    options.activeIndex = m_roiList ? m_roiList->currentRow() : 0;
    if (options.activeIndex < 0) {
        options.activeIndex = 0;
    }

    QPointer<ImageFindEditor> self = this;
    options.onRoiEdited = [self](int index, const CaptureRegion& region) {
        if (!self || !self->m_block) {
            return;
        }
        if (index < 0
            || index >= static_cast<int>(self->m_block->customRegionsWindowPercent.size())) {
            return;
        }
        self->m_block->customRegionsWindowPercent[static_cast<size_t>(index)] =
            storePickedCustomRegionPercent(region);
        self->m_block->customRegionsAnchoredToTargetWindow = true;
        self->m_block->customRegions.clear();
        self->m_block->customRegion = {};
        self->m_block->searchArea = SearchArea::CustomRegion;
        self->refreshRoiList();
        if (self->m_roiList && index >= 0 && index < self->m_roiList->count()) {
            self->m_roiList->blockSignals(true);
            self->m_roiList->setCurrentRow(index);
            self->m_roiList->blockSignals(false);
        }
        self->apply();
    };
    options.onRoiSelected = [self](int index) {
        if (!self || !self->m_roiList || index < 0 || index >= self->m_roiList->count()) {
            return;
        }
        self->m_roiList->blockSignals(true);
        self->m_roiList->setCurrentRow(index);
        self->m_roiList->blockSignals(false);
    };
    options.onConfirm = [self]() {
        if (self) {
            self->confirmFromRoiOverlay();
        }
    };
    options.onAdd = [self]() {
        if (self) {
            self->onPickRoi();
        }
    };
    options.onDelete = [self]() {
        if (self) {
            self->onRemoveRoi();
        }
    };
    return options;
}

void ImageFindEditor::confirmFromRoiOverlay() {
    syncUiToBlockValues();
    apply();
    dismissRoiPreviewOverlay();
    ScreenRegionOverlay::dismissAll();
    MatchTestOverlay::dismissAll();

    QDialog* closingDialog = m_embedded ? qobject_cast<QDialog*>(window()) : this;
    if (closingDialog) {
        closingDialog->accept();
    }
}

void ImageFindEditor::dismissRoiPreviewOverlay() {
    RoiPreviewOverlay::dismissAll();
    updateRoiPreviewButton(false);
}

void ImageFindEditor::updatePickRoiButton(bool pickActive) {
    if (!m_pickRoiButton) {
        return;
    }
    m_pickRoiButton->setText(pickActive ? tr("추가 취소") : tr("추가"));
}

void ImageFindEditor::updateMatchTestButton(bool overlayVisible) {
    if (!m_matchTestButton) {
        return;
    }
    m_matchTestButton->blockSignals(true);
    m_matchTestButton->setChecked(overlayVisible);
    m_matchTestButton->setText(overlayVisible ? tr("매칭 테스트 끄기") : tr("매칭 테스트"));
    m_matchTestButton->blockSignals(false);
}

void ImageFindEditor::onCaptureTemplate() {
    if (ScreenRegionOverlay::isPickActive()) {
        return;
    }

    MatchTestOverlay::dismissAll();
    updateMatchTestButton(false);
    RoiPreviewOverlay::dismissAll();
    updateRoiPreviewButton(false);

    QPointer<ImageFindEditor> self = this;
    ScreenRegionOverlay::StartPickOptions pickOptions;
    pickOptions.parkHostDuringPick = false;
    ScreenRegionOverlay::startPick(this, [self](bool accepted, const QRect& selectionRect) {
        if (!self) {
            return;
        }
        if (!accepted || selectionRect.width() < 2 || selectionRect.height() < 2) {
            return;
        }

        const cv::Mat captured = ScreenCapture::capturePhysicalRectForTemplate(
            selectionRect.x(), selectionRect.y(), selectionRect.width(), selectionRect.height());
        const QPoint dialogAnchor = QCursor::pos();

        ScreenRegionOverlay::deferUntilHostRestored([self, captured, selectionRect, dialogAnchor]() {
            if (!self) {
                return;
            }

            if (captured.empty() || ScreenCapture::isMostlyBlack(captured)) {
                QMessageBox::warning(self.data(), tr("템플릿 캡처"),
                                     tr("선택한 영역을 캡처하지 못했습니다.\n"
                                        "창 모드 또는 보더리스 창 모드를 사용해 보세요."));
                return;
            }

            CaptureConfirmDialog confirmDialog(captured, selectionRect, self->window());
            positionDialogNearGlobalPoint(confirmDialog, dialogAnchor);
            if (confirmDialog.exec() != QDialog::Accepted) {
                return;
            }

            const QString templatesDir =
                QDir(self->m_projectDirectory).filePath(QStringLiteral("templates"));
            QDir().mkpath(templatesDir);
            const QString fileName = QStringLiteral("capture_%1.png").arg(
                QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
            const QString absolutePath = QDir(templatesDir).filePath(fileName);
            if (!cv::imwrite(absolutePath.toStdString(), captured)) {
                QMessageBox::warning(self.data(), tr("템플릿 캡처"),
                                     tr("템플릿 이미지 저장에 실패했습니다."));
                return;
            }

            self->addTemplatePath(QDir(self->m_projectDirectory).relativeFilePath(absolutePath));
            self->apply();
        });
    }, pickOptions);
}

void ImageFindEditor::onRemoveTemplate() {
    if (!m_templateList || m_templateList->currentRow() < 0) {
        return;
    }
    delete m_templateList->takeItem(m_templateList->currentRow());
    syncBlockTemplatePathsFromList();
    if (m_templateList->count() > 0) {
        m_templateList->setCurrentRow(qMin(m_templateList->currentRow(), m_templateList->count() - 1));
    }
    if (m_removeTemplateButton) {
        m_removeTemplateButton->setEnabled(m_templateList->currentRow() >= 0);
    }
    updatePreview();
    apply();
}

void ImageFindEditor::startRoiPick() {
#ifdef _WIN32
    if (!ScreenCapture::findTargetWindow()) {
        QMessageBox::warning(this, tr("추가"), tr("대상 창을 찾을 수 없습니다. '창 지정' 또는 '서브 창으로 지정'을 사용하세요."));
        return;
    }
#endif

    RoiPreviewOverlay::dismissAll();
    updateRoiPreviewButton(false);

    QPointer<ImageFindEditor> self = this;
    m_roiPickActive = true;
    ScreenRegionOverlay::StartPickOptions pickOptions;
    pickOptions.parkHostDuringPick = false;
    if (m_block) {
        const std::vector<CaptureRegion> physicalRegions = physicalCustomRegionsForDisplay(*m_block);
        for (const CaptureRegion& physical : physicalRegions) {
            if (physical.width >= 2 && physical.height >= 2) {
                pickOptions.existingRoiPhysicalRects.emplace_back(
                    physical.x, physical.y, physical.width, physical.height);
            }
        }
    }
    ScreenRegionOverlay::startPick(this, [self](bool accepted, const QRect& selectionRect) {
        if (!self) {
            return;
        }
        self->m_roiPickActive = false;
        self->updatePickRoiButton(false);
        if (!self->m_block) {
            return;
        }
        if (!accepted || selectionRect.width() < 2 || selectionRect.height() < 2) {
            return;
        }

        CaptureRegion physical;
        physical.x = selectionRect.x();
        physical.y = selectionRect.y();
        physical.width = selectionRect.width();
        physical.height = selectionRect.height();

        self->m_block->searchArea = SearchArea::CustomRegion;
        self->m_block->customRegionsAnchoredToTargetWindow = true;
        self->m_block->customRegions.clear();
        self->m_block->customRegion = {};
        self->m_block->customRegionsWindowPercent.push_back(
            storePickedCustomRegionPercent(physical));
        self->refreshRoiList();
        if (self->m_roiList && self->m_roiList->count() > 0) {
            self->m_roiList->setCurrentRow(self->m_roiList->count() - 1);
        }
        self->apply();
        if (self->isTemplateCaptureHotkeyHookActive()) {
            self->showRoiPreviewOverlay(true);
        }
    }, pickOptions);

    if (ScreenRegionOverlay::isPickActive()) {
        updatePickRoiButton(true);
    } else {
        m_roiPickActive = false;
    }
}

void ImageFindEditor::onPickRoi() {
    if (m_roiPickActive && ScreenRegionOverlay::isPickActive()) {
        ScreenRegionOverlay::dismissAll();
        return;
    }
    startRoiPick();
}

void ImageFindEditor::onRemoveRoi() {
    if (!m_roiList || m_roiList->currentRow() < 0 || !m_block) {
        return;
    }
    const int row = m_roiList->currentRow();
    delete m_roiList->takeItem(row);
    if (row >= 0 && row < static_cast<int>(m_block->customRegionsWindowPercent.size())) {
        m_block->customRegionsWindowPercent.erase(
            m_block->customRegionsWindowPercent.begin() + row);
    }
    if (m_roiList->count() > 0) {
        m_roiList->setCurrentRow(qMin(m_roiList->currentRow(), m_roiList->count() - 1));
    }
    if (m_removeRoiButton) {
        m_removeRoiButton->setEnabled(m_roiList->currentRow() >= 0);
    }
    apply();
    if (hasConfiguredRoiForPreview() && isTemplateCaptureHotkeyHookActive()) {
        showRoiPreviewOverlay(true);
    } else {
        dismissRoiPreviewOverlay();
    }
}

void ImageFindEditor::onRoiPreview() {
    syncUiToBlockValues();
    if (!m_block) {
        return;
    }

    if (RoiPreviewOverlay::isVisible()) {
        dismissRoiPreviewOverlay();
        return;
    }

    showRoiPreviewOverlay(false);
}

void ImageFindEditor::onMatchTest() {
    syncUiToBlockValues();
    if (!m_block || !m_block->hasTemplates()) {
        QMessageBox::warning(this, tr("매칭 테스트"), tr("템플릿을 먼저 지정하세요."));
        return;
    }

    if (MatchTestOverlay::isVisible()) {
        MatchTestOverlay::hide();
        updateMatchTestButton(false);
        return;
    }

    RoiPreviewOverlay::dismissAll();
    updateRoiPreviewButton(false);

    MatchOptions options = m_block->matchOptions();
    options.threshold = m_thresholdSpin->value();

    const ImageFindMatchTestResult testResult = ImageFindBlock::testMatchTemplates(
        m_block->searchArea,
        m_block->percentRegion,
        m_block->templatePaths,
        options,
        m_projectDirectory.toStdString(),
        m_block->customRegionsWindowPercent);

    if (!testResult.captureOk) {
        QMessageBox::warning(this, tr("매칭 테스트"), QString::fromStdString(testResult.errorMessage));
        return;
    }

    QPointer<ImageFindEditor> self = this;
    const CaptureRegion physicalCustomRegion = physicalCustomRegionForDisplay(*m_block);
    const bool visible = MatchTestOverlay::show(testResult.matches,
                                                m_thresholdSpin->value(),
                                                m_block->searchArea,
                                                physicalCustomRegion,
                                                m_block->percentRegion,
                                                this,
                                                [self](bool overlayVisible) {
                                                    if (self) {
                                                        self->updateMatchTestButton(overlayVisible);
                                                    }
                                                },
                                                testResult.matchesPerCustomRegion.empty()
                                                    ? nullptr
                                                    : &testResult.matchesPerCustomRegion,
                                                testResult.matchDurationMs);
    updateMatchTestButton(visible);
}

void ImageFindEditor::updatePreview() {
    const QString absolute = selectedTemplateAbsolutePath();
    if (absolute.isEmpty()) {
        m_templatePreviewLabel->setText(tr("선택된 템플릿 없음"));
        m_templatePreviewLabel->setPixmap({});
        m_templatePreviewLabel->setToolTip({});
        if (m_templateSizeLabel) {
            m_templateSizeLabel->setText(tr("해상도: —"));
        }
        return;
    }

    QPixmap pixmap(absolute);
    if (pixmap.isNull()) {
        m_templatePreviewLabel->setText(tr("템플릿을 불러올 수 없습니다"));
        m_templatePreviewLabel->setPixmap({});
        m_templatePreviewLabel->setToolTip(absolute);
        if (m_templateSizeLabel) {
            m_templateSizeLabel->setText(tr("해상도: —"));
        }
        return;
    }

    // Scale to the label's fixed max slot — never use current widget size (layout/spin
    // drag can briefly change size() and make the thumbnail appear to grow/shrink).
    constexpr QSize kPreviewSlot(140, 105);
    m_templatePreviewLabel->setText({});
    m_templatePreviewLabel->setToolTip(absolute);
    m_templatePreviewLabel->setPixmap(
        pixmap.scaled(kPreviewSlot, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (m_templateSizeLabel) {
        m_templateSizeLabel->setText(
            tr("해상도: %1 × %2 px").arg(pixmap.width()).arg(pixmap.height()));
    }
}

void ImageFindEditor::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    if (!selectedTemplateAbsolutePath().isEmpty()) {
        updatePreview();
    }
}

void ImageFindEditor::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    syncTemplateCaptureHotkeyHook();
    if (isTemplateCaptureHotkeyHookActive()) {
        showRoiPreviewOverlay(true);
    }
}

void ImageFindEditor::hideEvent(QHideEvent* event) {
    stopTemplateHotkeyCapture();
    syncTemplateCaptureHotkeyHook();
    dismissRoiPreviewOverlay();
    QDialog::hideEvent(event);
}

bool ImageFindEditor::isTemplateCaptureHotkeyHookActive() const {
    if (!isVisible()) {
        return false;
    }

    QWidget* hostWindow = window();
    if (!hostWindow || !hostWindow->isVisible()) {
        return false;
    }

    if (!m_embedded) {
        return true;
    }

    if (const auto* stack = qobject_cast<const QStackedWidget*>(parent())) {
        return stack->currentWidget() == this;
    }

    return true;
}

void ImageFindEditor::refreshTemplateCaptureHotkeyHook() {
    syncTemplateCaptureHotkeyHook();
}

void ImageFindEditor::triggerTemplateCaptureFromHotkey() {
    onCaptureTemplate();
}

void ImageFindEditor::syncTemplateCaptureHotkeyHook() {
#ifdef _WIN32
    installTemplateCaptureHotkeyHookIfNeeded(this);
#else
    (void)this;
#endif
}

void ImageFindEditor::updateTemplateHotkeyDisplay() {
    if (!m_templateCaptureHotkeyLabel) {
        return;
    }
    const HotkeyBinding binding = TemplateCaptureHotkeySettings::binding();
    if (binding.isEmpty()) {
        m_templateCaptureHotkeyLabel->setText(tr("(없음 — 클릭하여 설정)"));
    } else {
        m_templateCaptureHotkeyLabel->setText(binding.displayString());
    }
    if (!m_capturingTemplateHotkey) {
        m_templateCaptureHotkeyLabel->setStyleSheet(hotkeyBindingLabelIdleStyleSheet(palette()));
    }
}

void ImageFindEditor::updateTemplateHotkeyCaptureUi() {
    if (!m_templateCaptureHotkeyLabel) {
        return;
    }
    if (m_capturingTemplateHotkey) {
        m_templateCaptureHotkeyLabel->setText(tr("입력 대기 중..."));
        m_templateCaptureHotkeyLabel->setStyleSheet(hotkeyBindingLabelCaptureStyleSheet(palette()));
    } else {
        updateTemplateHotkeyDisplay();
    }
}

void ImageFindEditor::startTemplateHotkeyCapture() {
    m_capturingTemplateHotkey = true;
    updateTemplateHotkeyCaptureUi();
    setFocus();
}

void ImageFindEditor::stopTemplateHotkeyCapture() {
    m_capturingTemplateHotkey = false;
    updateTemplateHotkeyCaptureUi();
}

void ImageFindEditor::applyTemplateHotkeyCapture(int virtualKey, Qt::KeyboardModifiers modifiers) {
    if (virtualKey == 0 || HotkeyBinding::isModifierOnlyVirtualKey(virtualKey)
        || HotkeyBinding::isMouseVirtualKey(virtualKey)) {
        return;
    }

    HotkeyBinding binding;
    binding.virtualKey = virtualKey;
    binding.ctrl = modifiers.testFlag(Qt::ControlModifier);
    binding.alt = modifiers.testFlag(Qt::AltModifier);
    binding.shift = modifiers.testFlag(Qt::ShiftModifier);
    TemplateCaptureHotkeySettings::setBinding(binding);
    stopTemplateHotkeyCapture();
    updateTemplateHotkeyDisplay();
    syncTemplateCaptureHotkeyHook();
}

bool ImageFindEditor::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_templateCaptureHotkeyLabel && event->type() == QEvent::MouseButtonPress) {
        startTemplateHotkeyCapture();
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void ImageFindEditor::keyPressEvent(QKeyEvent* event) {
    if (m_capturingTemplateHotkey) {
#ifdef _WIN32
        const int vk = HotkeyBinding::qtKeyToVirtualKey(event->key());
        if (vk != 0) {
            applyTemplateHotkeyCapture(vk, event->modifiers());
            event->accept();
            return;
        }
#else
        (void)event;
#endif
    }
    QDialog::keyPressEvent(event);
}

void ImageFindEditor::updateMatchFeedbackSummary() {
    if (!m_matchFeedbackSummary) {
        return;
    }
    const ClickPointerFeedbackSettings effective =
        m_useDefaultMatchFeedbackCheck && m_useDefaultMatchFeedbackCheck->isChecked()
            ? PointerFeedbackSettings::click()
            : m_draftMatchFeedback;
    QString summary = ClickPointerFeedbackSettingsDialog::settingsSummary(effective);
    if (m_useDefaultMatchFeedbackCheck && m_useDefaultMatchFeedbackCheck->isChecked()) {
        summary += tr(" (기본 녹색/빨간색 펄스)");
    }
    m_matchFeedbackSummary->setText(summary);
}

void ImageFindEditor::onOpenMatchFeedbackSettings() {
    ClickPointerFeedbackSettingsDialog dialog(this);
    dialog.setDialogKind(PointerFeedbackDialogKind::Detection);
    dialog.setPersistToGlobalSettingsOnAccept(false);
    dialog.loadFromSettings(m_draftMatchFeedback);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_draftMatchFeedback = dialog.acceptedSettings();
    if (m_useDefaultMatchFeedbackCheck) {
        m_useDefaultMatchFeedbackCheck->setChecked(false);
        m_matchFeedbackButton->setEnabled(true);
    }
    updateMatchFeedbackSummary();
}
