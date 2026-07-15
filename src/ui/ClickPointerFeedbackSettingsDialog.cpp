#include "ui/ClickPointerFeedbackSettingsDialog.h"

#include "app/PointerFeedbackSettings.h"
#include "ui/UiResizeHandle.h"
#include "ui/UiStrings.h"
#include "ui/widgets/ClickPointerFeedbackPreviewWidget.h"
#include "ui/widgets/DragAdjustDoubleSpinBox.h"
#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/widgets/HintLabel.h"

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

constexpr auto kDialogSplitterSettingsKey = "pointerFeedback/click/dialogSplitter";

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

QString ClickPointerFeedbackSettingsDialog::shapeDisplayName(ClickPointerFeedbackShape shape) {
    switch (shape) {
    case ClickPointerFeedbackShape::FilledDotRings:
        return QObject::tr("원형 (점 + 링)");
    case ClickPointerFeedbackShape::RingOnly:
        return QObject::tr("링만");
    case ClickPointerFeedbackShape::Crosshair:
        return QObject::tr("십자선");
    case ClickPointerFeedbackShape::Square:
        return QObject::tr("사각형");
    }
    return QObject::tr("원형 (점 + 링)");
}

QString ClickPointerFeedbackSettingsDialog::settingsSummary(const ClickPointerFeedbackSettings& settings) {
    return QObject::tr("%1 · %2 ms · %3배")
        .arg(shapeDisplayName(settings.shape))
        .arg(settings.displayDurationMs)
        .arg(settings.animationSpeed, 0, 'f', 2);
}

ClickPointerFeedbackSettingsDialog::ClickPointerFeedbackSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("마우스 클릭 피드백 애니메이션"));
    setModal(true);
    resize(640, 560);
    setupUi();
    loadFromSettings(PointerFeedbackSettings::click());
    connect(this, &QDialog::finished, this, [this](int) {
        if (m_bodySplitter) {
            QSettings().setValue(QLatin1String(kDialogSplitterSettingsKey), m_bodySplitter->saveState());
        }
    });
}

void ClickPointerFeedbackSettingsDialog::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setSpacing(12);

    auto* hint = new HintLabel(
        tr("기능 편집의 실행 위치 표시가 켜진 기능에서 마우스 클릭 시 대상 창에 표시되는 포인터 애니메이션입니다."),
        this);
    hint->setWordWrap(true);
    outerLayout->addWidget(hint);

    auto* bodySplitter = new QSplitter(Qt::Horizontal, this);
    UiResizeHandle::configureSplitter(bodySplitter);
    m_bodySplitter = bodySplitter;

    auto* previewPane = new QWidget(bodySplitter);
    auto* previewColumn = new QVBoxLayout(previewPane);
    previewColumn->setContentsMargins(0, 0, 0, 0);
    previewColumn->setSpacing(8);

    auto* previewFrame = new QFrame(previewPane);
    previewFrame->setObjectName(QStringLiteral("clickFeedbackPreviewFrame"));
    previewFrame->setFrameShape(QFrame::StyledPanel);
    auto* previewFrameLayout = new QVBoxLayout(previewFrame);
    previewFrameLayout->setContentsMargins(10, 10, 10, 10);
    previewFrameLayout->setSpacing(8);

    m_preview = new ClickPointerFeedbackPreviewWidget(previewFrame);
    previewFrameLayout->addWidget(m_preview);

    m_replayButton = new QPushButton(tr("다시 재생"), previewFrame);
    m_replayButton->setCursor(Qt::PointingHandCursor);
    connect(m_replayButton, &QPushButton::clicked, m_preview, &ClickPointerFeedbackPreviewWidget::restartAnimation);
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
    m_durationSpin->setRange(100, 3000);
    m_durationSpin->setSingleStep(10);
    timingForm->addRow(makeFormLabel(tr("표시 시간"), timingGroup),
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

    m_shapeCombo = new QComboBox(lookGroup);
    m_shapeCombo->addItem(shapeDisplayName(ClickPointerFeedbackShape::FilledDotRings),
                          static_cast<int>(ClickPointerFeedbackShape::FilledDotRings));
    m_shapeCombo->addItem(shapeDisplayName(ClickPointerFeedbackShape::RingOnly),
                          static_cast<int>(ClickPointerFeedbackShape::RingOnly));
    m_shapeCombo->addItem(shapeDisplayName(ClickPointerFeedbackShape::Crosshair),
                          static_cast<int>(ClickPointerFeedbackShape::Crosshair));
    m_shapeCombo->addItem(shapeDisplayName(ClickPointerFeedbackShape::Square),
                          static_cast<int>(ClickPointerFeedbackShape::Square));
    lookForm->addRow(makeFormLabel(tr("모양"), lookGroup), m_shapeCombo);

    m_colorButton = new QPushButton(lookGroup);
    m_colorButton->setCursor(Qt::PointingHandCursor);
    m_colorButton->setToolTip(tr("클릭 피드백 색상 선택"));
    connect(m_colorButton, &QPushButton::clicked, this, &ClickPointerFeedbackSettingsDialog::onPickColor);
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

    m_coreSizeLabel = makeFormLabel(tr("중심 크기"), sizeGroup);
    m_coreSizeSpin = new DragAdjustSpinBox(sizeGroup);
    m_coreSizeSpin->setRange(2, 16);
    m_coreSizeSpin->setSingleStep(1);
    m_coreSizeField = makeUnitRow(m_coreSizeSpin, tr("px"), sizeGroup);
    sizeForm->addRow(m_coreSizeLabel, m_coreSizeField);

    m_maxExpandSpin = new DragAdjustSpinBox(sizeGroup);
    m_maxExpandSpin->setRange(8, 80);
    m_maxExpandSpin->setSingleStep(1);
    sizeForm->addRow(makeFormLabel(tr("최대 확장"), sizeGroup),
                     makeUnitRow(m_maxExpandSpin, tr("px"), sizeGroup));

    m_ringCountLabel = makeFormLabel(tr("링 개수"), sizeGroup);
    m_ringCountSpin = new DragAdjustSpinBox(sizeGroup);
    m_ringCountSpin->setRange(0, 4);
    m_ringCountSpin->setSingleStep(1);
    m_ringCountField = m_ringCountSpin;
    sizeForm->addRow(m_ringCountLabel, m_ringCountSpin);

    m_ringThicknessLabel = makeFormLabel(tr("선 두께"), sizeGroup);
    m_ringThicknessSpin = new DragAdjustSpinBox(sizeGroup);
    m_ringThicknessSpin->setRange(1, 8);
    m_ringThicknessSpin->setSingleStep(1);
    m_ringThicknessField = makeUnitRow(m_ringThicknessSpin, tr("px"), sizeGroup);
    sizeForm->addRow(m_ringThicknessLabel, m_ringThicknessField);
    formColumn->addWidget(sizeGroup);
    formColumn->addStretch();

    scroll->setWidget(scrollContent);
    bodySplitter->addWidget(scroll);
    bodySplitter->setStretchFactor(0, 4);
    bodySplitter->setStretchFactor(1, 6);
    bodySplitter->setSizes({260, 360});
    const QByteArray splitterState =
        QSettings().value(QLatin1String(kDialogSplitterSettingsKey)).toByteArray();
    if (!splitterState.isEmpty()) {
        bodySplitter->restoreState(splitterState);
    }
    outerLayout->addWidget(bodySplitter, 1);

    const auto hookPreview = [this]() { applyDraftToPreview(); };
    connect(m_durationSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_speedSpin, &DragAdjustDoubleSpinBox::valueChanged, this, hookPreview);
    connect(m_shapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateFieldVisibility();
        applyDraftToPreview();
        if (m_preview) {
            m_preview->restartAnimation();
        }
    });
    connect(m_coreSizeSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_maxExpandSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_ringCountSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_ringThicknessSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);
    connect(m_maxAlphaSpin, &DragAdjustSpinBox::valueChanged, this, hookPreview);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    auto* resetButton = buttons->addButton(tr("기본값"), QDialogButtonBox::ResetRole);
    connect(resetButton, &QPushButton::clicked, this, &ClickPointerFeedbackSettingsDialog::onResetDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        PointerFeedbackSettings::setClick(readDraftFromUi());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outerLayout->addWidget(buttons);
}

void ClickPointerFeedbackSettingsDialog::loadFromSettings(const ClickPointerFeedbackSettings& settings) {
    m_durationSpin->setValue(settings.displayDurationMs);
    m_speedSpin->setValue(settings.animationSpeed);
    const int shapeIndex = m_shapeCombo->findData(static_cast<int>(settings.shape));
    m_shapeCombo->setCurrentIndex(shapeIndex >= 0 ? shapeIndex : 0);
    m_color = settings.color;
    updateColorButton();
    m_coreSizeSpin->setValue(settings.coreSize);
    m_maxExpandSpin->setValue(settings.maxExpandRadius);
    m_ringCountSpin->setValue(settings.ringCount);
    m_ringThicknessSpin->setValue(settings.ringThickness);
    m_maxAlphaSpin->setValue(settings.maxAlpha);
    updateFieldVisibility();
    applyDraftToPreview();
    if (m_preview) {
        m_preview->restartAnimation();
    }
}

ClickPointerFeedbackSettings ClickPointerFeedbackSettingsDialog::readDraftFromUi() const {
    ClickPointerFeedbackSettings settings;
    settings.displayDurationMs = m_durationSpin->value();
    settings.animationSpeed = m_speedSpin->value();
    settings.shape = static_cast<ClickPointerFeedbackShape>(m_shapeCombo->currentData().toInt());
    settings.color = m_color;
    settings.coreSize = m_coreSizeSpin->value();
    settings.maxExpandRadius = m_maxExpandSpin->value();
    settings.ringCount = m_ringCountSpin->value();
    settings.ringThickness = m_ringThicknessSpin->value();
    settings.maxAlpha = m_maxAlphaSpin->value();
    return settings;
}

void ClickPointerFeedbackSettingsDialog::applyDraftToPreview() {
    if (!m_preview) {
        return;
    }
    m_preview->setSettings(readDraftFromUi());
}

void ClickPointerFeedbackSettingsDialog::updateFieldVisibility() {
    const auto shape = static_cast<ClickPointerFeedbackShape>(m_shapeCombo->currentData().toInt());
    const bool usesRings = shape == ClickPointerFeedbackShape::FilledDotRings
                           || shape == ClickPointerFeedbackShape::RingOnly
                           || shape == ClickPointerFeedbackShape::Square;
    const bool usesThickness = usesRings || shape == ClickPointerFeedbackShape::Crosshair;
    const bool usesCore = shape != ClickPointerFeedbackShape::RingOnly;

    setFormRowVisible(m_ringCountLabel, m_ringCountField, usesRings);
    setFormRowVisible(m_ringThicknessLabel, m_ringThicknessField, usesThickness);
    setFormRowVisible(m_coreSizeLabel, m_coreSizeField, usesCore);
}

void ClickPointerFeedbackSettingsDialog::updateColorButton() {
    m_colorButton->setText(m_color.name(QColor::HexRgb).toUpper());
    m_colorButton->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; color: %2; border: 1px solid palette(mid);"
                       " padding: 4px 10px; min-height: 24px; border-radius: 4px; }")
            .arg(m_color.name(QColor::HexRgb),
                 m_color.lightness() > 140 ? QStringLiteral("#111318") : QStringLiteral("#f5f7fa")));
}

void ClickPointerFeedbackSettingsDialog::onPickColor() {
    const QColor picked =
        QColorDialog::getColor(m_color, this, tr("클릭 피드백 색상"), QColorDialog::DontUseNativeDialog);
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

void ClickPointerFeedbackSettingsDialog::onResetDefaults() {
    loadFromSettings(PointerFeedbackSettings::defaultClick());
}
