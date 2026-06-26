#include "ui/editors/ImageFindEditor.h"

#include "app/TemplateCaptureHotkeySettings.h"
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
#include <QResizeEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QSizePolicy>

#include <opencv2/imgcodecs.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

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
    m_roiPickReplaceRow = -1;
    updatePickRoiButton(false);
    updateEditRoiButton(false);
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
    if (m_block->customRegions.empty() && m_block->customRegion.width >= 2
        && m_block->customRegion.height >= 2) {
        m_block->customRegions.push_back(m_block->customRegion);
    }
    refreshRoiList();
    updatePreview();
    updateMatchTestButton(MatchTestOverlay::isVisible());
    updateRoiPreviewButton(RoiPreviewOverlay::isVisible());
    updateTemplateHotkeyDisplay();
    syncTemplateCaptureHotkeyHook();
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
    connect(m_pollIntervalSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        const int snapped = snapImageFindPollIntervalMs(value);
        if (snapped != value) {
            QSignalBlocker blocker(m_pollIntervalSpin);
            m_pollIntervalSpin->setValue(snapped);
        }
    });

    auto* pollIntervalRow = new QWidget(this);
    auto* pollIntervalLayout = new QHBoxLayout(pollIntervalRow);
    pollIntervalLayout->setContentsMargins(0, 0, 0, 0);
    pollIntervalLayout->setSpacing(4);
    pollIntervalLayout->addWidget(m_pollIntervalSpin);
    pollIntervalLayout->addWidget(new QLabel(QStringLiteral("ms"), pollIntervalRow));

    auto* matchGroup = new QGroupBox(tr("매칭 설정"), this);
    auto* matchForm = new QFormLayout(matchGroup);
    matchForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    matchForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    matchForm->setHorizontalSpacing(12);
    matchForm->setVerticalSpacing(8);
    matchForm->addRow(tr("임계값"), m_thresholdSpin);
    matchForm->addRow(tr("탐지 재시도"), pollIntervalRow);

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
    m_editRoiButton = new QPushButton(tr("수정"), this);
    m_editRoiButton->setToolTip(tr("목록에서 선택한 ROI 영역을 다시 지정합니다. 다시 누르면 취소합니다."));
    m_removeRoiButton = new QPushButton(tr("삭제"), this);
    m_removeRoiButton->setToolTip(tr("목록에서 선택한 ROI를 제거합니다."));
    m_roiPreviewButton = new QPushButton(tr("미리보기"), this);
    m_roiPreviewButton->setCheckable(true);
    m_roiPreviewButton->setToolTip(tr("대상 창 위에 탐색 ROI를 반투명 오버레이로 표시합니다."));

    auto* roiToolbar = new QHBoxLayout();
    roiToolbar->setSpacing(8);
    roiToolbar->addWidget(m_roiPreviewButton);
    roiToolbar->addWidget(m_pickRoiButton);
    roiToolbar->addWidget(m_editRoiButton);
    roiToolbar->addWidget(m_removeRoiButton);
    roiToolbar->addStretch(1);

    auto* roiHint = new HintLabel(tr("ROI가 없으면 대상 창 전체에서 탐색합니다."), this);

    auto* roiGroup = new QGroupBox(tr("탐색 ROI"), this);
    auto* roiLayout = new QVBoxLayout(roiGroup);
    roiLayout->setSpacing(8);
    roiLayout->addWidget(roiHint);
    roiLayout->addWidget(m_roiList);
    roiLayout->addLayout(roiToolbar);

    layout->addWidget(matchGroup);
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
    connect(m_editRoiButton, &QPushButton::clicked, this, &ImageFindEditor::onEditRoi);
    connect(m_removeRoiButton, &QPushButton::clicked, this, &ImageFindEditor::onRemoveRoi);
    connect(m_roiList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_removeRoiButton) {
            m_removeRoiButton->setEnabled(row >= 0);
        }
        if (m_editRoiButton) {
            m_editRoiButton->setEnabled(row >= 0);
        }
        syncRoiPreviewSelection();
    });
    connect(m_roiPreviewButton, &QPushButton::clicked, this, &ImageFindEditor::onRoiPreview);
    connect(clearHotkeyButton, &QPushButton::clicked, this, [this]() {
        stopTemplateHotkeyCapture();
        TemplateCaptureHotkeySettings::setBinding({});
        updateTemplateHotkeyDisplay();
        syncTemplateCaptureHotkeyHook();
    });
    connect(m_thresholdSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        updatePreview();
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
    m_block->templateMatchMode =
        m_matchModeSwitch->isRightSelected() ? ImageFindTemplateMatchMode::All : ImageFindTemplateMatchMode::Any;
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
    if (!m_block || !m_roiList) {
        return;
    }
    m_block->customRegions.clear();
    for (int row = 0; row < m_roiList->count(); ++row) {
        const QListWidgetItem* item = m_roiList->item(row);
        if (!item) {
            continue;
        }
        const QRect rect = item->data(Qt::UserRole).toRect();
        if (rect.width() < 2 || rect.height() < 2) {
            continue;
        }
        CaptureRegion region;
        region.x = rect.x();
        region.y = rect.y();
        region.width = rect.width();
        region.height = rect.height();
        m_block->customRegions.push_back(region);
    }
    if (!m_block->customRegions.empty()) {
        m_block->customRegion = m_block->customRegions.front();
        m_block->searchArea = SearchArea::CustomRegion;
    } else {
        m_block->customRegion = {};
    }
}

void ImageFindEditor::refreshRoiList() {
    if (!m_roiList || !m_block) {
        return;
    }

    QRect selectedRect;
    if (m_roiList->currentRow() >= 0 && m_roiList->currentItem()) {
        selectedRect = m_roiList->currentItem()->data(Qt::UserRole).toRect();
    }

    m_roiList->blockSignals(true);
    m_roiList->clear();
    int index = 1;
    for (const CaptureRegion& region : m_block->customRegions) {
        if (region.width < 2 || region.height < 2) {
            continue;
        }
        const QRect rect(region.x, region.y, region.width, region.height);
        const QString label =
            tr("ROI %1: %2, %3 — %4×%5")
                .arg(index++)
                .arg(region.x)
                .arg(region.y)
                .arg(region.width)
                .arg(region.height);
        auto* item = new QListWidgetItem(label, m_roiList);
        item->setData(Qt::UserRole, rect);
        item->setToolTip(label);
    }
    if (!selectedRect.isNull()) {
        for (int row = 0; row < m_roiList->count(); ++row) {
            if (m_roiList->item(row)->data(Qt::UserRole).toRect() == selectedRect) {
                m_roiList->setCurrentRow(row);
                break;
            }
        }
    } else if (m_roiList->count() > 0) {
        m_roiList->setCurrentRow(0);
    }
    m_roiList->blockSignals(false);
    if (m_removeRoiButton) {
        m_removeRoiButton->setEnabled(m_roiList->currentRow() >= 0);
    }
    if (m_editRoiButton) {
        m_editRoiButton->setEnabled(m_roiList->currentRow() >= 0);
    }
    syncRoiPreviewSelection();
}

int ImageFindEditor::selectedRoiPreviewIndex() const {
    if (!m_roiList || m_roiList->count() <= 0) {
        return 1;
    }
    const int row = m_roiList->currentRow();
    return row >= 0 ? row + 1 : 0;
}

void ImageFindEditor::syncRoiPreviewSelection() {
    if (!RoiPreviewOverlay::isVisible()) {
        return;
    }
    RoiPreviewOverlay::setSelectedRoiIndex(selectedRoiPreviewIndex());
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
    for (const CaptureRegion& region : m_block->customRegions) {
        if (region.width >= 2 && region.height >= 2) {
            return true;
        }
    }
    if (m_block->searchArea == SearchArea::CustomRegion && m_block->customRegion.width >= 2
        && m_block->customRegion.height >= 2) {
        return true;
    }
    if (m_block->searchArea == SearchArea::ScreenPercent && m_block->percentRegion.width > 0.0
        && m_block->percentRegion.height > 0.0) {
        return true;
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
                                    "메인 창에서 먼저 '창 지정'을 사용하세요."));
        }
        return;
    }
#endif

    if (RoiPreviewOverlay::isVisible()) {
        RoiPreviewOverlay::dismissAll();
    }

    QPointer<ImageFindEditor> self = this;
    const bool interactive = true;
    const bool visible = RoiPreviewOverlay::show(
        m_block->searchArea,
        m_block->customRegion,
        m_block->percentRegion,
        m_block->customRegions,
        this,
        [self](bool overlayVisible) {
            if (self) {
                self->updateRoiPreviewButton(overlayVisible);
            }
        },
        selectedRoiPreviewIndex(),
        interactive,
        [self](int roiIndex) {
            if (!self || !self->m_roiList) {
                return;
            }
            const int row = roiIndex - 1;
            if (row >= 0 && row < self->m_roiList->count()) {
                self->m_roiList->setCurrentRow(row);
                self->syncRoiPreviewSelection();
            }
        },
        [self]() {
            if (self) {
                self->onPickRoi();
            }
        },
        [self](int roiIndex) {
            if (!self || !self->m_roiList) {
                return;
            }
            const int row = roiIndex - 1;
            if (row >= 0 && row < self->m_roiList->count()) {
                self->m_roiList->setCurrentRow(row);
                self->onEditRoi();
            }
        },
        [self](int roiIndex) {
            if (!self || !self->m_roiList) {
                return;
            }
            const int row = roiIndex - 1;
            if (row >= 0 && row < self->m_roiList->count()) {
                self->m_roiList->setCurrentRow(row);
                self->onRemoveRoi();
            }
        });
    updateRoiPreviewButton(visible);
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

void ImageFindEditor::updateEditRoiButton(bool pickActive) {
    if (!m_editRoiButton) {
        return;
    }
    m_editRoiButton->setText(pickActive ? tr("수정 취소") : tr("수정"));
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

void ImageFindEditor::startRoiPick(int replaceListRow) {
#ifdef _WIN32
    if (!ScreenCapture::findTargetWindow()) {
        QMessageBox::warning(this,
                             replaceListRow >= 0 ? tr("수정") : tr("추가"),
                             tr("대상 창을 찾을 수 없습니다. 먼저 '창 지정'을 사용하세요."));
        return;
    }
#endif

    RoiPreviewOverlay::dismissAll();
    updateRoiPreviewButton(false);

    QPointer<ImageFindEditor> self = this;
    m_roiPickActive = true;
    m_roiPickReplaceRow = replaceListRow;
    ScreenRegionOverlay::StartPickOptions pickOptions;
    pickOptions.parkHostDuringPick = false;
    if (m_block) {
        for (const CaptureRegion& region : m_block->customRegions) {
            if (region.width >= 2 && region.height >= 2) {
                pickOptions.existingRoiPhysicalRects.emplace_back(
                    region.x, region.y, region.width, region.height);
            }
        }
    }
    ScreenRegionOverlay::startPick(this, [self](bool accepted, const QRect& selectionRect) {
        if (!self) {
            return;
        }
        const int replaceRow = self->m_roiPickReplaceRow;
        self->m_roiPickActive = false;
        self->m_roiPickReplaceRow = -1;
        self->updatePickRoiButton(false);
        self->updateEditRoiButton(false);
        if (!self->m_block) {
            return;
        }
        if (!accepted || selectionRect.width() < 2 || selectionRect.height() < 2) {
            return;
        }

        CaptureRegion region;
        region.x = selectionRect.x();
        region.y = selectionRect.y();
        region.width = selectionRect.width();
        region.height = selectionRect.height();

        self->m_block->searchArea = SearchArea::CustomRegion;
        if (replaceRow >= 0 && replaceRow < static_cast<int>(self->m_block->customRegions.size())) {
            self->m_block->customRegions[static_cast<size_t>(replaceRow)] = region;
        } else {
            self->m_block->customRegions.push_back(region);
        }
        if (!self->m_block->customRegions.empty()) {
            self->m_block->customRegion = self->m_block->customRegions.front();
        }
        self->refreshRoiList();
        if (self->m_roiList && self->m_roiList->count() > 0) {
            const int selectRow =
                replaceRow >= 0 ? qMin(replaceRow, self->m_roiList->count() - 1)
                                : self->m_roiList->count() - 1;
            self->m_roiList->setCurrentRow(selectRow);
        }
        self->apply();
        if (self->isTemplateCaptureHotkeyHookActive()) {
            self->showRoiPreviewOverlay(true);
        }
    }, pickOptions);

    if (ScreenRegionOverlay::isPickActive()) {
        if (replaceListRow >= 0) {
            updateEditRoiButton(true);
        } else {
            updatePickRoiButton(true);
        }
    } else {
        m_roiPickActive = false;
        m_roiPickReplaceRow = -1;
    }
}

void ImageFindEditor::onPickRoi() {
    if (m_roiPickActive && ScreenRegionOverlay::isPickActive()) {
        ScreenRegionOverlay::dismissAll();
        return;
    }
    startRoiPick(-1);
}

void ImageFindEditor::onEditRoi() {
    if (!m_roiList || m_roiList->currentRow() < 0) {
        return;
    }
    if (m_roiPickActive && ScreenRegionOverlay::isPickActive()) {
        ScreenRegionOverlay::dismissAll();
        return;
    }
    startRoiPick(m_roiList->currentRow());
}

void ImageFindEditor::onRemoveRoi() {
    if (!m_roiList || m_roiList->currentRow() < 0 || !m_block) {
        return;
    }
    delete m_roiList->takeItem(m_roiList->currentRow());
    syncBlockCustomRegionsFromList();
    if (m_roiList->count() > 0) {
        m_roiList->setCurrentRow(qMin(m_roiList->currentRow(), m_roiList->count() - 1));
    }
    if (m_removeRoiButton) {
        m_removeRoiButton->setEnabled(m_roiList->currentRow() >= 0);
    }
    if (m_editRoiButton) {
        m_editRoiButton->setEnabled(m_roiList->currentRow() >= 0);
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
        m_block->customRegion,
        m_block->percentRegion,
        m_block->customRegions,
        m_block->templatePaths,
        options,
        m_projectDirectory.toStdString());

    if (!testResult.captureOk) {
        QMessageBox::warning(this, tr("매칭 테스트"), QString::fromStdString(testResult.errorMessage));
        return;
    }

    QPointer<ImageFindEditor> self = this;
    const bool visible = MatchTestOverlay::show(testResult.matches,
                                                m_thresholdSpin->value(),
                                                m_block->searchArea,
                                                m_block->customRegion,
                                                m_block->percentRegion,
                                                this,
                                                [self](bool overlayVisible) {
                                                    if (self) {
                                                        self->updateMatchTestButton(overlayVisible);
                                                    }
                                                },
                                                testResult.matchesPerCustomRegion.empty()
                                                    ? nullptr
                                                    : &testResult.matchesPerCustomRegion);
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

    m_templatePreviewLabel->setText({});
    m_templatePreviewLabel->setToolTip(absolute);
    m_templatePreviewLabel->setPixmap(
        pixmap.scaled(m_templatePreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
