#include "ui/WindowSelectionFeedbackSettingsDialog.h"

#include "app/PointerFeedbackSettings.h"
#include "ui/UiResizeHandle.h"
#include "ui/UiStrings.h"
#include "ui/widgets/DragAdjustDoubleSpinBox.h"
#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/widgets/HintLabel.h"
#include "ui/widgets/WindowSelectionFeedbackPreviewWidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

constexpr auto kDialogSplitterSettingsKey = "pointerFeedback/windowSelection/dialogSplitter";

QLabel* makeFormLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
}

QWidget* makeUnitRow(QWidget* input, const QString& unit, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(input);
    layout->addWidget(new QLabel(unit, row));
    layout->addStretch();
    return row;
}

void setFormRowVisible(QWidget* label, QWidget* field, bool visible) {
    if (label) {
        label->setVisible(visible);
    }
    if (field) {
        field->setVisible(visible);
    }
}

} // namespace

QString WindowSelectionFeedbackSettingsDialog::styleDisplayName(WindowSelectionFeedbackStyle style) {
    switch (style) {
    case WindowSelectionFeedbackStyle::LockOn:
        return QObject::tr("락온 (레이더 핑 + 브래킷)");
    case WindowSelectionFeedbackStyle::RadarPing:
        return QObject::tr("레이더 핑");
    case WindowSelectionFeedbackStyle::BorderGlow:
        return QObject::tr("테두리 글로우");
    case WindowSelectionFeedbackStyle::ClassicFill:
        return QObject::tr("원형 채우기 (클래식)");
    }
    return QObject::tr("락온 (레이더 핑 + 브래킷)");
}

QString WindowSelectionFeedbackSettingsDialog::settingsSummary(const WindowSelectionFeedbackSettings& settings) {
    if (!settings.enabled) {
        return QObject::tr("끔");
    }
    return QObject::tr("%1 · %2 ms · %3배")
        .arg(styleDisplayName(settings.style))
        .arg(settings.displayDurationMs)
        .arg(settings.animationSpeed, 0, 'f', 2);
}

WindowSelectionFeedbackSettingsDialog::WindowSelectionFeedbackSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("타겟 지정 애니메이션"));
    setModal(true);
    resize(660, 580);
    setupUi();
    loadFromSettings(PointerFeedbackSettings::windowSelection());
    connect(this, &QDialog::finished, this, [this](int) {
        if (m_bodySplitter) {
            QSettings().setValue(QLatin1String(kDialogSplitterSettingsKey), m_bodySplitter->saveState());
        }
    });
}

void WindowSelectionFeedbackSettingsDialog::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setSpacing(12);

    auto* hint = new HintLabel(
        tr("타겟 지정 또는 창 목록에서 타겟을 선택했을 때 타겟 위에 재생되는 확인 애니메이션입니다."),
        this);
    hint->setWordWrap(true);
    outerLayout->addWidget(hint);

    m_enabledCheck = new QCheckBox(tr("타겟 지정 애니메이션 사용"), this);
    connect(m_enabledCheck, &QCheckBox::toggled, this, [this](bool) {
        updateFieldVisibility();
        applyDraftToPreview();
    });
    outerLayout->addWidget(m_enabledCheck);

    auto* bodySplitter = new QSplitter(Qt::Horizontal, this);
    UiResizeHandle::configureSplitter(bodySplitter);
    m_bodySplitter = bodySplitter;

    auto* previewPane = new QWidget(bodySplitter);
    auto* previewColumn = new QVBoxLayout(previewPane);
    previewColumn->setContentsMargins(0, 0, 0, 0);
    previewColumn->setSpacing(8);

    auto* previewFrame = new QFrame(previewPane);
    previewFrame->setObjectName(QStringLiteral("windowSelectionFeedbackPreviewFrame"));
    previewFrame->setFrameShape(QFrame::StyledPanel);
    auto* previewFrameLayout = new QVBoxLayout(previewFrame);
    previewFrameLayout->setContentsMargins(10, 10, 10, 10);
    previewFrameLayout->setSpacing(8);

    m_preview = new WindowSelectionFeedbackPreviewWidget(previewFrame);
    previewFrameLayout->addWidget(m_preview);

    m_replayButton = new QPushButton(tr("다시 재생"), previewFrame);
    m_replayButton->setCursor(Qt::PointingHandCursor);
    connect(m_replayButton, &QPushButton::clicked, m_preview, &WindowSelectionFeedbackPreviewWidget::restartAnimation);
    previewFrameLayout->addWidget(m_replayButton, 0, Qt::AlignHCenter);

    previewColumn->addWidget(previewFrame);
    previewColumn->addStretch();
    bodySplitter->addWidget(previewPane);

    auto* scroll = new QScrollArea(bodySplitter);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* scrollContent = new QWidget(scroll);
    auto* formColumn = new QVBoxLayout(scrollContent);
    formColumn->setSpacing(10);

    auto* timingGroup = new QGroupBox(tr("타이밍"), scrollContent);
    auto* timingForm = new QFormLayout(timingGroup);
    timingForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    timingForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_durationSpin = new DragAdjustSpinBox(timingGroup);
    m_durationSpin->setRange(400, 3000);
    m_durationSpin->setSingleStep(50);
    timingForm->addRow(makeFormLabel(tr("재생 시간"), timingGroup),
                       makeUnitRow(m_durationSpin, tr("ms"), timingGroup));

    m_speedSpin = new DragAdjustDoubleSpinBox(timingGroup);
    m_speedSpin->setRange(0.25, 4.0);
    m_speedSpin->setSingleStep(0.05);
    m_speedSpin->setDecimals(2);
    timingForm->addRow(makeFormLabel(tr("애니메이션 속도"), timingGroup),
                       makeUnitRow(m_speedSpin, tr("배"), timingGroup));
    formColumn->addWidget(timingGroup);

    auto* lookGroup = new QGroupBox(tr("모양 및 색상"), scrollContent);
    auto* lookForm = new QFormLayout(lookGroup);
    lookForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    lookForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_styleCombo = new QComboBox(lookGroup);
    m_styleCombo->addItem(styleDisplayName(WindowSelectionFeedbackStyle::LockOn),
                          static_cast<int>(WindowSelectionFeedbackStyle::LockOn));
    m_styleCombo->addItem(styleDisplayName(WindowSelectionFeedbackStyle::RadarPing),
                          static_cast<int>(WindowSelectionFeedbackStyle::RadarPing));
    m_styleCombo->addItem(styleDisplayName(WindowSelectionFeedbackStyle::BorderGlow),
                          static_cast<int>(WindowSelectionFeedbackStyle::BorderGlow));
    m_styleCombo->addItem(styleDisplayName(WindowSelectionFeedbackStyle::ClassicFill),
                          static_cast<int>(WindowSelectionFeedbackStyle::ClassicFill));
    lookForm->addRow(makeFormLabel(tr("스타일"), lookGroup), m_styleCombo);

    m_colorButton = new QPushButton(lookGroup);
    m_colorButton->setCursor(Qt::PointingHandCursor);
    m_colorButton->setToolTip(tr("애니메이션 색상 선택"));
    connect(m_colorButton, &QPushButton::clicked, this, &WindowSelectionFeedbackSettingsDialog::onPickColor);
    lookForm->addRow(makeFormLabel(tr("색상"), lookGroup), m_colorButton);

    m_maxAlphaSpin = new DragAdjustSpinBox(lookGroup);
    m_maxAlphaSpin->setRange(40, 255);
    m_maxAlphaSpin->setSingleStep(5);
    lookForm->addRow(makeFormLabel(tr("최대 불투명도"), lookGroup), m_maxAlphaSpin);
    formColumn->addWidget(lookGroup);

    auto* sizeGroup = new QGroupBox(tr("크기"), scrollContent);
    auto* sizeForm = new QFormLayout(sizeGroup);
    sizeForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    sizeForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_pingRingWidthLabel = makeFormLabel(tr("파동 링 두께"), sizeGroup);
    m_pingRingWidthSpin = new DragAdjustSpinBox(sizeGroup);
    m_pingRingWidthSpin->setRange(8, 48);
    m_pingRingWidthSpin->setSingleStep(1);
    m_pingRingWidthField = makeUnitRow(m_pingRingWidthSpin, tr("px"), sizeGroup);
    sizeForm->addRow(m_pingRingWidthLabel, m_pingRingWidthField);

    m_edgeGlowWidthLabel = makeFormLabel(tr("테두리 글로우"), sizeGroup);
    m_edgeGlowWidthSpin = new DragAdjustSpinBox(sizeGroup);
    m_edgeGlowWidthSpin->setRange(8, 32);
    m_edgeGlowWidthSpin->setSingleStep(1);
    m_edgeGlowWidthField = makeUnitRow(m_edgeGlowWidthSpin, tr("px"), sizeGroup);
    sizeForm->addRow(m_edgeGlowWidthLabel, m_edgeGlowWidthField);

    m_bracketScaleLabel = makeFormLabel(tr("브래킷 크기"), sizeGroup);
    m_bracketScaleSpin = new DragAdjustSpinBox(sizeGroup);
    m_bracketScaleSpin->setRange(50, 150);
    m_bracketScaleSpin->setSingleStep(5);
    m_bracketScaleField = makeUnitRow(m_bracketScaleSpin, tr("%"), sizeGroup);
    sizeForm->addRow(m_bracketScaleLabel, m_bracketScaleField);
    formColumn->addWidget(sizeGroup);

    auto* effectsGroup = new QGroupBox(tr("효과 (락온 스타일)"), scrollContent);
    m_effectsGroup = effectsGroup;
    auto* effectsForm = new QFormLayout(effectsGroup);
    effectsForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    effectsForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_echoRingLabel = makeFormLabel(tr("잔향 링"), effectsGroup);
    m_echoRingCheck = new QCheckBox(effectsGroup);
    m_echoRingField = m_echoRingCheck;
    effectsForm->addRow(m_echoRingLabel, m_echoRingCheck);

    m_centerBloomLabel = makeFormLabel(tr("중앙 플래시"), effectsGroup);
    m_centerBloomCheck = new QCheckBox(effectsGroup);
    m_centerBloomField = m_centerBloomCheck;
    effectsForm->addRow(m_centerBloomLabel, m_centerBloomCheck);

    m_edgeGlowToggleLabel = makeFormLabel(tr("테두리 글로우"), effectsGroup);
    m_edgeGlowCheck = new QCheckBox(effectsGroup);
    m_edgeGlowToggleField = m_edgeGlowCheck;
    effectsForm->addRow(m_edgeGlowToggleLabel, m_edgeGlowCheck);

    m_cornerBracketsLabel = makeFormLabel(tr("모서리 브래킷"), effectsGroup);
    m_cornerBracketsCheck = new QCheckBox(effectsGroup);
    m_cornerBracketsField = m_cornerBracketsCheck;
    effectsForm->addRow(m_cornerBracketsLabel, m_cornerBracketsCheck);
    formColumn->addWidget(effectsGroup);
    formColumn->addStretch();

    scroll->setWidget(scrollContent);
    bodySplitter->addWidget(scroll);
    bodySplitter->setStretchFactor(0, 4);
    bodySplitter->setStretchFactor(1, 6);
    bodySplitter->setSizes({260, 380});
    const QByteArray splitterState =
        QSettings().value(QLatin1String(kDialogSplitterSettingsKey)).toByteArray();
    if (!splitterState.isEmpty()) {
        bodySplitter->restoreState(splitterState);
    }
    outerLayout->addWidget(bodySplitter, 1);

    const auto hookPreview = [this]() { applyDraftToPreview(); };
    connect(m_durationSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_speedSpin, &DragAdjustDoubleSpinBox::valueChanged, this, hookPreview);
    connect(m_styleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateFieldVisibility();
        applyDraftToPreview();
        if (m_preview) {
            m_preview->restartAnimation();
        }
    });
    connect(m_pingRingWidthSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_edgeGlowWidthSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_bracketScaleSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_maxAlphaSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_echoRingCheck, &QCheckBox::toggled, this, hookPreview);
    connect(m_centerBloomCheck, &QCheckBox::toggled, this, hookPreview);
    connect(m_edgeGlowCheck, &QCheckBox::toggled, this, hookPreview);
    connect(m_cornerBracketsCheck, &QCheckBox::toggled, this, hookPreview);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    auto* resetButton = buttons->addButton(tr("기본값"), QDialogButtonBox::ResetRole);
    connect(resetButton, &QPushButton::clicked, this, &WindowSelectionFeedbackSettingsDialog::onResetDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        PointerFeedbackSettings::setWindowSelection(readDraftFromUi());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outerLayout->addWidget(buttons);
}

void WindowSelectionFeedbackSettingsDialog::loadFromSettings(const WindowSelectionFeedbackSettings& settings) {
    m_enabledCheck->setChecked(settings.enabled);
    m_durationSpin->setValue(settings.displayDurationMs);
    m_speedSpin->setValue(settings.animationSpeed);
    const int styleIndex = m_styleCombo->findData(static_cast<int>(settings.style));
    m_styleCombo->setCurrentIndex(styleIndex >= 0 ? styleIndex : 0);
    m_color = settings.color;
    updateColorButton();
    m_pingRingWidthSpin->setValue(settings.pingRingWidth);
    m_edgeGlowWidthSpin->setValue(settings.edgeGlowWidth);
    m_bracketScaleSpin->setValue(settings.bracketScalePercent);
    m_maxAlphaSpin->setValue(settings.maxAlpha);
    m_echoRingCheck->setChecked(settings.echoRing);
    m_centerBloomCheck->setChecked(settings.centerBloom);
    m_edgeGlowCheck->setChecked(settings.edgeGlow);
    m_cornerBracketsCheck->setChecked(settings.cornerBrackets);
    updateFieldVisibility();
    applyDraftToPreview();
    if (m_preview) {
        m_preview->restartAnimation();
    }
}

WindowSelectionFeedbackSettings WindowSelectionFeedbackSettingsDialog::readDraftFromUi() const {
    WindowSelectionFeedbackSettings settings;
    settings.enabled = m_enabledCheck->isChecked();
    settings.displayDurationMs = m_durationSpin->value();
    settings.animationSpeed = m_speedSpin->value();
    settings.style = static_cast<WindowSelectionFeedbackStyle>(m_styleCombo->currentData().toInt());
    settings.color = m_color;
    settings.pingRingWidth = m_pingRingWidthSpin->value();
    settings.edgeGlowWidth = m_edgeGlowWidthSpin->value();
    settings.bracketScalePercent = m_bracketScaleSpin->value();
    settings.maxAlpha = m_maxAlphaSpin->value();
    settings.echoRing = m_echoRingCheck->isChecked();
    settings.centerBloom = m_centerBloomCheck->isChecked();
    settings.edgeGlow = m_edgeGlowCheck->isChecked();
    settings.cornerBrackets = m_cornerBracketsCheck->isChecked();
    return settings;
}

void WindowSelectionFeedbackSettingsDialog::applyDraftToPreview() {
    if (!m_preview) {
        return;
    }
    m_preview->setSettings(readDraftFromUi());
}

void WindowSelectionFeedbackSettingsDialog::updateFieldVisibility() {
    const bool enabled = m_enabledCheck->isChecked();
    const auto style = static_cast<WindowSelectionFeedbackStyle>(m_styleCombo->currentData().toInt());

    m_preview->setEnabled(enabled);
    m_replayButton->setEnabled(enabled);
    m_durationSpin->setEnabled(enabled);
    m_speedSpin->setEnabled(enabled);
    m_styleCombo->setEnabled(enabled);
    m_colorButton->setEnabled(enabled);
    m_maxAlphaSpin->setEnabled(enabled);

    const bool lockOn = style == WindowSelectionFeedbackStyle::LockOn;
    const bool usesRing = style == WindowSelectionFeedbackStyle::LockOn
                          || style == WindowSelectionFeedbackStyle::RadarPing
                          || style == WindowSelectionFeedbackStyle::ClassicFill;
    const bool usesEdgeGlow = style == WindowSelectionFeedbackStyle::LockOn
                              || style == WindowSelectionFeedbackStyle::BorderGlow;
    const bool usesBrackets = lockOn;
    const bool usesEffects = lockOn && enabled;

    setFormRowVisible(m_pingRingWidthLabel, m_pingRingWidthField, enabled && usesRing);
    m_pingRingWidthSpin->setEnabled(enabled && usesRing);

    setFormRowVisible(m_edgeGlowWidthLabel, m_edgeGlowWidthField, enabled && usesEdgeGlow);
    m_edgeGlowWidthSpin->setEnabled(enabled && usesEdgeGlow);

    setFormRowVisible(m_bracketScaleLabel, m_bracketScaleField, enabled && usesBrackets);
    m_bracketScaleSpin->setEnabled(enabled && usesBrackets);

    setFormRowVisible(m_echoRingLabel, m_echoRingField, usesEffects);
    m_echoRingCheck->setEnabled(usesEffects);
    setFormRowVisible(m_centerBloomLabel, m_centerBloomField, usesEffects);
    m_centerBloomCheck->setEnabled(usesEffects);
    setFormRowVisible(m_edgeGlowToggleLabel, m_edgeGlowToggleField, usesEffects);
    m_edgeGlowCheck->setEnabled(usesEffects);
    setFormRowVisible(m_cornerBracketsLabel, m_cornerBracketsField, usesEffects);
    m_cornerBracketsCheck->setEnabled(usesEffects);

    if (m_effectsGroup) {
        m_effectsGroup->setVisible(enabled && lockOn);
    }
}

void WindowSelectionFeedbackSettingsDialog::updateColorButton() {
    m_colorButton->setText(m_color.name(QColor::HexRgb).toUpper());
    m_colorButton->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; color: %2; border: 1px solid palette(mid);"
                       " padding: 4px 10px; min-height: 24px; border-radius: 4px; }")
            .arg(m_color.name(QColor::HexRgb),
                 m_color.lightness() > 140 ? QStringLiteral("#111318") : QStringLiteral("#f5f7fa")));
}

void WindowSelectionFeedbackSettingsDialog::onPickColor() {
    const QColor picked =
        QColorDialog::getColor(m_color, this, tr("타겟 지정 애니메이션 색상"), QColorDialog::DontUseNativeDialog);
    if (!picked.isValid()) {
        return;
    }
    m_color = picked;
    updateColorButton();
    applyDraftToPreview();
    if (m_preview) {
        m_preview->restartAnimation();
    }
}

void WindowSelectionFeedbackSettingsDialog::onResetDefaults() {
    loadFromSettings(PointerFeedbackSettings::defaultWindowSelection());
}
