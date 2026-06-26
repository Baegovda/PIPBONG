#include "ui/editors/WorkflowLoopRegionsDialog.h"

#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/UiStrings.h"
#include "ui/widgets/HintLabel.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

WorkflowLoopRegionEditDialog::WorkflowLoopRegionEditDialog(int blockCount,
                                                           const WorkflowLoopRegion* existing,
                                                           int defaultStartBlock,
                                                           int defaultEndBlock,
                                                           bool lockBlockRange,
                                                           QWidget* parent)
    : QDialog(parent)
    , m_blockCount(std::max(1, blockCount))
    , m_lockBlockRange(lockBlockRange) {
    if (existing) {
        m_region = *existing;
    } else {
        m_region.startIndex = std::clamp(defaultStartBlock - 1, 0, m_blockCount - 1);
        m_region.endIndex = std::clamp(defaultEndBlock - 1, m_region.startIndex, m_blockCount - 1);
    }

    setWindowTitle(existing ? tr("구간 반복 편집") : tr("구간 반복 추가"));
    setModal(true);
    setupUi();
}

void WorkflowLoopRegionEditDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_startSpin = new DragAdjustSpinBox(this);
    m_startSpin->setRange(1, m_blockCount);
    m_startSpin->setSingleStep(1);
    m_startSpin->setValue(m_region.startIndex + 1);
    m_startSpin->setToolTip(tr("반복 구간의 첫 번째 블록 번호 (# 열)"));

    m_endSpin = new DragAdjustSpinBox(this);
    m_endSpin->setRange(1, m_blockCount);
    m_endSpin->setSingleStep(1);
    m_endSpin->setValue(m_region.endIndex + 1);
    m_endSpin->setToolTip(tr("반복 구간의 마지막 블록 번호 (# 열)"));

    m_rangeLabel = new QLabel(this);
    m_rangeLabel->setText(tr("#%1 ~ #%2").arg(m_region.startIndex + 1).arg(m_region.endIndex + 1));

    if (m_lockBlockRange) {
        form->addRow(tr("선택 구간"), m_rangeLabel);
    } else {
        form->addRow(tr("시작 블록"), m_startSpin);
        form->addRow(tr("끝 블록"), m_endSpin);
    }

    m_exitConditionCombo = new QComboBox(this);
    m_exitConditionCombo->addItem(tr("탐지 실패 시까지"));
    m_exitConditionCombo->addItem(tr("탐지 성공 시까지"));
    m_exitConditionCombo->addItem(tr("직전 매칭 성공 시까지"));
    m_exitConditionCombo->addItem(tr("직전 매칭 실패 시까지"));
    m_exitConditionCombo->setCurrentIndex(comboIndexForExitCondition(m_region.exitCondition));
    form->addRow(tr("종료 조건"), m_exitConditionCombo);

    m_missLimitLabel = new QLabel(tr("탐지 실패 허용"), this);
    m_missLimitSpin = new DragAdjustSpinBox(this);
    m_missLimitSpin->setRange(1, 1000);
    m_missLimitSpin->setSingleStep(1);
    m_missLimitSpin->setValue(m_region.detectionMissLimit);
    m_missLimitSpin->setSuffix(tr(" 회"));
    form->addRow(m_missLimitLabel, m_missLimitSpin);

    layout->addLayout(form);

    connect(m_startSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_lockBlockRange) {
            return;
        }
        if (m_endSpin->value() < value) {
            m_endSpin->setValue(value);
        }
        m_endSpin->setMinimum(value);
    });
    connect(m_exitConditionCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateConditionDependentUi(); });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        int startBlock = m_region.startIndex + 1;
        int endBlock = m_region.endIndex + 1;
        if (m_lockBlockRange) {
            startBlock = m_region.startIndex + 1;
            endBlock = m_region.endIndex + 1;
        } else {
            startBlock = m_startSpin->value();
            endBlock = std::max(startBlock, m_endSpin->value());
        }
        m_region.startIndex = startBlock - 1;
        m_region.endIndex = endBlock - 1;
        m_region.exitCondition = exitConditionFromComboIndex(m_exitConditionCombo->currentIndex());
        m_region.detectionMissLimit = m_missLimitSpin->value();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    updateConditionDependentUi();
}

LoopExitCondition WorkflowLoopRegionEditDialog::exitConditionFromComboIndex(int index) const {
    switch (index) {
    case 1:
        return LoopExitCondition::DetectionSucceeded;
    case 2:
        return LoopExitCondition::LastMatchSuccess;
    case 3:
        return LoopExitCondition::LastMatchFailed;
    default:
        return LoopExitCondition::DetectionFailed;
    }
}

int WorkflowLoopRegionEditDialog::comboIndexForExitCondition(LoopExitCondition condition) const {
    switch (condition) {
    case LoopExitCondition::DetectionSucceeded:
        return 1;
    case LoopExitCondition::LastMatchSuccess:
        return 2;
    case LoopExitCondition::LastMatchFailed:
        return 3;
    default:
        return 0;
    }
}

void WorkflowLoopRegionEditDialog::updateConditionDependentUi() {
    const bool showMissLimit =
        exitConditionFromComboIndex(m_exitConditionCombo->currentIndex())
        == LoopExitCondition::DetectionFailed;
    m_missLimitLabel->setVisible(showMissLimit);
    m_missLimitSpin->setVisible(showMissLimit);
}

WorkflowLoopRegion WorkflowLoopRegionEditDialog::resultRegion() const {
    return m_region;
}

WorkflowLoopRegionsDialog::WorkflowLoopRegionsDialog(Workflow* workflow,
                                                     int defaultStartBlock,
                                                     int defaultEndBlock,
                                                     QWidget* parent)
    : QDialog(parent)
    , m_workflow(workflow)
    , m_defaultStartBlock(defaultStartBlock)
    , m_defaultEndBlock(defaultEndBlock) {
    setWindowTitle(tr("구간 반복"));
    setModal(true);
    resize(520, 360);
    setupUi();
    refreshList();
}

void WorkflowLoopRegionsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* hint = new HintLabel(
        tr("워크플로우 목록에서 구간 반복을 누른 뒤 블록을 드래그하여 범위를 선택하세요. "
           "여기서는 기존 구간을 편집·삭제하거나 번호로 직접 추가할 수 있습니다. 구간은 서로 "
           "겹칠 수 없습니다."),
        this);
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setMinimumHeight(180);
    layout->addWidget(m_list, 1);

    auto* buttonRow = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("추가"), this);
    auto* editButton = new QPushButton(tr("편집"), this);
    auto* deleteButton = new QPushButton(tr("삭제"), this);
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(editButton);
    buttonRow->addWidget(deleteButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    if (QPushButton* closeButton = buttons->button(QDialogButtonBox::Close)) {
        closeButton->setText(tr("닫기"));
    }
    layout->addWidget(buttons);

    connect(addButton, &QPushButton::clicked, this, &WorkflowLoopRegionsDialog::addRegion);
    connect(editButton, &QPushButton::clicked, this, &WorkflowLoopRegionsDialog::editRegion);
    connect(deleteButton, &QPushButton::clicked, this, &WorkflowLoopRegionsDialog::deleteRegion);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &WorkflowLoopRegionsDialog::editRegion);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString WorkflowLoopRegionsDialog::regionListLabel(const WorkflowLoopRegion& region) const {
    return tr("#%1~#%2 · %3")
        .arg(region.startIndex + 1)
        .arg(region.endIndex + 1)
        .arg(QString::fromStdString(loopExitConditionDisplayLabel(region.exitCondition)));
}

void WorkflowLoopRegionsDialog::refreshList() {
    m_list->clear();
    if (!m_workflow) {
        return;
    }
    for (const WorkflowLoopRegion& region : m_workflow->loopRegions()) {
        auto* item = new QListWidgetItem(regionListLabel(region), m_list);
        item->setData(Qt::UserRole, QString::fromStdString(region.id));
    }
}

void WorkflowLoopRegionsDialog::addRegion() {
    if (!m_workflow) {
        return;
    }
    const int blockCount = static_cast<int>(m_workflow->blocks().size());
    if (blockCount <= 0) {
        QMessageBox::information(this, tr("구간 반복"), tr("워크플로우에 블록이 없습니다."));
        return;
    }

    WorkflowLoopRegionEditDialog dialog(blockCount,
                                        nullptr,
                                        m_defaultStartBlock,
                                        m_defaultEndBlock,
                                        this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::vector<WorkflowLoopRegion> regions = m_workflow->loopRegions();
    regions.push_back(dialog.resultRegion());
    m_workflow->setLoopRegions(std::move(regions));
    refreshList();
}

void WorkflowLoopRegionsDialog::editRegion() {
    if (!m_workflow) {
        return;
    }
    const QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return;
    }
    const QString regionId = item->data(Qt::UserRole).toString();
    std::vector<WorkflowLoopRegion> regions = m_workflow->loopRegions();
    const auto it = std::find_if(regions.begin(), regions.end(), [&](const WorkflowLoopRegion& region) {
        return QString::fromStdString(region.id) == regionId;
    });
    if (it == regions.end()) {
        return;
    }

    WorkflowLoopRegionEditDialog dialog(static_cast<int>(m_workflow->blocks().size()), &(*it), 1, 1, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    *it = dialog.resultRegion();
    m_workflow->setLoopRegions(std::move(regions));
    refreshList();
}

void WorkflowLoopRegionsDialog::deleteRegion() {
    if (!m_workflow) {
        return;
    }
    const QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return;
    }
    const QString regionId = item->data(Qt::UserRole).toString();
    std::vector<WorkflowLoopRegion> regions = m_workflow->loopRegions();
    regions.erase(std::remove_if(regions.begin(),
                                 regions.end(),
                                 [&](const WorkflowLoopRegion& region) {
                                     return QString::fromStdString(region.id) == regionId;
                                 }),
                  regions.end());
    m_workflow->setLoopRegions(std::move(regions));
    refreshList();
}
