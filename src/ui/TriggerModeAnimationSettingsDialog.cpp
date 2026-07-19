#include "ui/TriggerModeAnimationSettingsDialog.h"

#include "ui/TriggerListAnimationRenderer.h"
#include "ui/widgets/DragAdjustDoubleSpinBox.h"
#include "ui/widgets/TriggerListAnimationPreviewWidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

QWidget* makeStateEditorPage(QWidget* parent) {
    return new QWidget(parent);
}

TriggerListStateAnimationSettings& stateRef(TriggerModeListAnimationSettings& settings,
                                            TriggerAnimationState state) {
    switch (state) {
    case TriggerAnimationState::Watch:
        return settings.watch;
    case TriggerAnimationState::Cooldown:
        return settings.cooldown;
    case TriggerAnimationState::Action:
        return settings.action;
    }
    return settings.watch;
}

const TriggerListStateAnimationSettings& stateRef(const TriggerModeListAnimationSettings& settings,
                                                 TriggerAnimationState state) {
    switch (state) {
    case TriggerAnimationState::Watch:
        return settings.watch;
    case TriggerAnimationState::Cooldown:
        return settings.cooldown;
    case TriggerAnimationState::Action:
        return settings.action;
    }
    return settings.watch;
}

} // namespace

TriggerModeAnimationSettingsDialog::TriggerModeAnimationSettingsDialog(
    const TriggerModeListAnimationSettings& settings, QWidget* parent)
    : QDialog(parent)
    , m_draft(clampTriggerModeListAnimations(settings)) {
    setWindowTitle(tr("트리거 목록 애니메이션"));
    setModal(true);
    resize(560, 460);
    setupUi();
    if (m_stateList && m_stateList->count() > 0) {
        m_stateList->setCurrentRow(0);
    }
}

TriggerModeListAnimationSettings TriggerModeAnimationSettingsDialog::settings() const {
    return m_draft;
}

void TriggerModeAnimationSettingsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    m_preview = new TriggerListAnimationPreviewWidget(this);
    layout->addWidget(m_preview);

    auto* body = new QHBoxLayout();
    m_stateList = new QListWidget(this);
    m_stateList->addItem(tr("감시"));
    m_stateList->addItem(tr("쿨다운"));
    m_stateList->addItem(tr("동작"));
    m_stateList->setFixedWidth(96);
    body->addWidget(m_stateList);

    m_stateStack = new QStackedWidget(this);
    for (int i = 0; i < 3; ++i) {
        auto* page = makeStateEditorPage(m_stateStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->addStretch();
        m_stateStack->addWidget(page);
    }
    body->addWidget(m_stateStack, 1);

    auto* editorHost = new QWidget(this);
    auto* editorLayout = new QVBoxLayout(editorHost);
    editorLayout->setContentsMargins(0, 0, 0, 0);

    auto* form = new QFormLayout();
    m_styleCombo = new QComboBox(editorHost);
    form->addRow(tr("애니메이션"), m_styleCombo);

    auto* colorRow = new QWidget(editorHost);
    auto* colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    m_colorButton = new QPushButton(tr("강조 색상"), colorRow);
    auto* resetColorButton = new QPushButton(tr("기본값"), colorRow);
    colorLayout->addWidget(m_colorButton);
    colorLayout->addWidget(resetColorButton);
    colorLayout->addStretch();
    form->addRow(QString(), colorRow);

    m_speedSpin = new DragAdjustDoubleSpinBox(editorHost);
    m_speedSpin->setRange(0.25, 3.0);
    m_speedSpin->setSingleStep(0.05);
    m_speedSpin->setDecimals(2);
    m_speedSpin->setSuffix(QStringLiteral("x"));
    form->addRow(tr("속도"), m_speedSpin);

    m_showSonarRipplesCheck = new QCheckBox(tr("소나 링"), editorHost);
    m_showRadarSweepCheck = new QCheckBox(tr("회전 스윕"), editorHost);
    auto* sonarRow = new QWidget(editorHost);
    auto* sonarLayout = new QHBoxLayout(sonarRow);
    sonarLayout->setContentsMargins(0, 0, 0, 0);
    sonarLayout->addWidget(m_showSonarRipplesCheck);
    sonarLayout->addWidget(m_showRadarSweepCheck);
    sonarLayout->addStretch();
    form->addRow(QString(), sonarRow);

    editorLayout->addLayout(form);
    editorLayout->addStretch();
    body->addWidget(editorHost, 1);
    layout->addLayout(body, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* resetButton = new QPushButton(tr("전체 기본값"), this);
    buttons->addButton(resetButton, QDialogButtonBox::ResetRole);
    layout->addWidget(buttons);

    connect(m_stateList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) {
            return;
        }
        applyCurrentEditorToDraft();
        loadStateEditor(row);
    });
    connect(m_styleCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        applyCurrentEditorToDraft();
        updateSonarOptionVisibility();
        updatePreview();
    });
    connect(m_speedSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this]() {
        applyCurrentEditorToDraft();
        updatePreview();
    });
    connect(m_showSonarRipplesCheck, &QCheckBox::toggled, this, [this]() {
        applyCurrentEditorToDraft();
        updatePreview();
    });
    connect(m_showRadarSweepCheck, &QCheckBox::toggled, this, [this]() {
        applyCurrentEditorToDraft();
        updatePreview();
    });
    connect(m_colorButton, &QPushButton::clicked, this, &TriggerModeAnimationSettingsDialog::onPickAccentColor);
    connect(resetColorButton, &QPushButton::clicked, this, [this]() {
        applyCurrentEditorToDraft();
        const auto state = static_cast<TriggerAnimationState>(m_stateList->currentRow());
        stateRef(m_draft, state).accentColor = QColor();
        loadStateEditor(m_stateList->currentRow());
    });
    connect(resetButton, &QPushButton::clicked, this, &TriggerModeAnimationSettingsDialog::onResetDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        applyCurrentEditorToDraft();
        m_draft = clampTriggerModeListAnimations(m_draft);
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void TriggerModeAnimationSettingsDialog::populateStyleCombo(QComboBox* combo,
                                                            TriggerAnimationState state) {
    combo->clear();
    const auto addStyle = [&](TriggerListAnimationStyle style) {
        combo->addItem(triggerListAnimationStyleDisplayName(style), static_cast<int>(style));
    };
    switch (state) {
    case TriggerAnimationState::Watch:
        addStyle(TriggerListAnimationStyle::SonarRadar);
        addStyle(TriggerListAnimationStyle::StaticBullseye);
        addStyle(TriggerListAnimationStyle::SpinningArc);
        addStyle(TriggerListAnimationStyle::PulseRing);
        break;
    case TriggerAnimationState::Cooldown:
        addStyle(TriggerListAnimationStyle::CountdownText);
        addStyle(TriggerListAnimationStyle::SpinningArc);
        addStyle(TriggerListAnimationStyle::PulseRing);
        addStyle(TriggerListAnimationStyle::StaticBullseye);
        addStyle(TriggerListAnimationStyle::SonarRadar);
        break;
    case TriggerAnimationState::Action:
        addStyle(TriggerListAnimationStyle::DefaultPrism);
        addStyle(TriggerListAnimationStyle::PulseRing);
        addStyle(TriggerListAnimationStyle::SonarRadar);
        addStyle(TriggerListAnimationStyle::SpinningArc);
        break;
    }
}

void TriggerModeAnimationSettingsDialog::loadStateEditor(int stateIndex) {
    m_loadingStateIndex = stateIndex;
    const auto state = static_cast<TriggerAnimationState>(stateIndex);
    const TriggerListStateAnimationSettings& current = stateRef(m_draft, state);

    populateStyleCombo(m_styleCombo, state);
    const int styleIndex =
        m_styleCombo->findData(static_cast<int>(current.style));
    m_styleCombo->setCurrentIndex(styleIndex >= 0 ? styleIndex : 0);
    m_speedSpin->setValue(current.speed);
    m_showSonarRipplesCheck->setChecked(current.showSonarRipples);
    m_showRadarSweepCheck->setChecked(current.showRadarSweep);

    if (current.accentColor.isValid()) {
        m_colorButton->setStyleSheet(QStringLiteral("background-color: %1;").arg(current.accentColor.name()));
    } else {
        m_colorButton->setStyleSheet(QString());
    }

    updateSonarOptionVisibility();
    updatePreview();
    m_loadingStateIndex = -1;
}

void TriggerModeAnimationSettingsDialog::applyCurrentEditorToDraft() {
    if (!m_stateList || m_loadingStateIndex >= 0) {
        return;
    }
    const int row = m_stateList->currentRow();
    if (row < 0) {
        return;
    }
    const auto state = static_cast<TriggerAnimationState>(row);
    TriggerListStateAnimationSettings& target = stateRef(m_draft, state);
    target.style = static_cast<TriggerListAnimationStyle>(m_styleCombo->currentData().toInt());
    target.speed = m_speedSpin->value();
    target.showSonarRipples = m_showSonarRipplesCheck->isChecked();
    target.showRadarSweep = m_showRadarSweepCheck->isChecked();
    m_draft = clampTriggerModeListAnimations(m_draft);
}

void TriggerModeAnimationSettingsDialog::updatePreview() {
    if (!m_preview || !m_stateList) {
        return;
    }
    const int row = m_stateList->currentRow();
    if (row < 0) {
        return;
    }
    const auto state = static_cast<TriggerAnimationState>(row);
    m_preview->setState(state);
    m_preview->setSettings(stateRef(m_draft, state));
}

void TriggerModeAnimationSettingsDialog::updateSonarOptionVisibility() {
    const bool sonar = m_styleCombo
                       && m_styleCombo->currentData().toInt()
                              == static_cast<int>(TriggerListAnimationStyle::SonarRadar);
    if (m_showSonarRipplesCheck) {
        m_showSonarRipplesCheck->setVisible(sonar);
    }
    if (m_showRadarSweepCheck) {
        m_showRadarSweepCheck->setVisible(sonar);
    }
}

void TriggerModeAnimationSettingsDialog::onPickAccentColor() {
    applyCurrentEditorToDraft();
    const int row = m_stateList->currentRow();
    if (row < 0) {
        return;
    }
    const auto state = static_cast<TriggerAnimationState>(row);
    TriggerListStateAnimationSettings& target = stateRef(m_draft, state);
    const QColor initial = target.accentColor.isValid()
                               ? target.accentColor
                               : TriggerListAnimationRenderer::resolvedAccentColor(state, target);
    const QColor picked =
        QColorDialog::getColor(initial, this, tr("강조 색상"), QColorDialog::ShowAlphaChannel);
    if (!picked.isValid()) {
        return;
    }
    target.accentColor = picked;
    loadStateEditor(row);
}

void TriggerModeAnimationSettingsDialog::onResetDefaults() {
    m_draft = TriggerModeListAnimationSettings::defaults();
    loadStateEditor(m_stateList->currentRow());
}
