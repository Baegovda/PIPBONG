#include "ui/ProfileEditDialog.h"

#include "core/capture/ScreenCapture.h"
#include "core/capture/WindowPicker.h"
#include "ui/TargetWindowBindingRole.h"
#include "ui/TargetWindowHighlightOverlay.h"
#include "ui/TargetWindowListPicker.h"
#include "ui/UiStrings.h"
#include "ui/UiThemeColors.h"
#include "ui/widgets/HintLabel.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <optional>

#ifdef _WIN32
#include <windows.h>
#endif

ProfileEditDialog::ProfileEditDialog(const QString& profileName,
                                     const QString& targetWindowTitle,
                                     const QString& subTargetWindowTitle,
                                     bool defaultProfile,
                                     bool fixedDefaultProfile,
                                     const QString& currentTargetWindowTitle,
                                     QWidget* parent)
    : QDialog(parent)
    , m_fixedDefaultProfile(fixedDefaultProfile) {
    setWindowTitle(fixedDefaultProfile ? tr("기본 프로필") : tr("프로필 편집"));
    setModal(true);
    setupUi(currentTargetWindowTitle);
    if (m_nameEdit) {
        m_nameEdit->setText(profileName);
    }
    if (m_fixedDefaultNameLabel) {
        m_fixedDefaultNameLabel->setText(QStringLiteral("기본"));
    }
    if (m_targetWindowTitleEdit) {
        m_targetWindowTitleEdit->setText(targetWindowTitle);
    }
    if (m_subTargetWindowTitleEdit) {
        m_subTargetWindowTitleEdit->setText(subTargetWindowTitle);
    }
    if (m_defaultProfileCheck) {
        m_defaultProfileCheck->setChecked(defaultProfile);
    }
    updateDefaultProfileUi();
}

ProfileEditDialog::Result ProfileEditDialog::result() const {
    Result out;
    if (m_fixedDefaultProfile) {
        out.name = QStringLiteral("기본");
        out.defaultProfile = true;
        out.targetWindowTitle = QString();
        out.subTargetWindowTitle = QString();
        return out;
    }
    out.name = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
    out.defaultProfile = m_defaultProfileCheck && m_defaultProfileCheck->isChecked();
    out.targetWindowTitle =
        out.defaultProfile || !m_targetWindowTitleEdit ? QString() : m_targetWindowTitleEdit->text().trimmed();
    out.subTargetWindowTitle =
        out.defaultProfile || !m_subTargetWindowTitleEdit
            ? QString()
            : m_subTargetWindowTitleEdit->text().trimmed();
    return out;
}

void ProfileEditDialog::setupUi(const QString& currentTargetWindowTitle) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    if (m_fixedDefaultProfile) {
        auto* hint = new HintLabel(
            tr("시스템 기본 프로필입니다. 이름·타겟·프로필 순서는 변경할 수 없으며, 목록 맨 위에 고정됩니다."),
            this);
        layout->addWidget(hint);

        auto* profileGroup = new QGroupBox(tr("프로필"), this);
        profileGroup->setToolTip(tr("기본 프로필은 전역으로 동작하며 타겟을 지정하지 않습니다."));
        auto* profileLayout = new QVBoxLayout(profileGroup);
        profileLayout->setSpacing(6);

        m_fixedDefaultNameLabel = new QLabel(QStringLiteral("기본"), profileGroup);
        m_fixedDefaultNameLabel->setObjectName(QStringLiteral("fixedDefaultProfileName"));
        m_fixedDefaultNameLabel->setStyleSheet(QStringLiteral(
            "QLabel#fixedDefaultProfileName {"
            "  font-weight: 600;"
            "  padding: 6px 10px;"
            "  border-radius: 6px;"
            "  background-color: palette(button);"
            "  color: palette(windowText);"
            "}"));
        profileLayout->addWidget(m_fixedDefaultNameLabel);
        layout->addWidget(profileGroup);
    } else {
        auto* intro = new QLabel(
            tr("옵션 위에 마우스를 올리면 자세한 설명이 표시됩니다."), this);
        intro->setWordWrap(true);
        {
            QPalette pal = intro->palette();
            pal.setColor(QPalette::WindowText, secondaryHintTextColor(pal));
            intro->setPalette(pal);
        }
        layout->addWidget(intro);

        auto* profileGroup = new QGroupBox(tr("프로필"), this);
        profileGroup->setToolTip(tr("프로필 이름과 시작 시 기본으로 열 프로필입니다."));
        auto* profileForm = new QFormLayout(profileGroup);
        profileForm->setSpacing(6);

        m_nameEdit = new QLineEdit(profileGroup);
        m_nameEdit->setPlaceholderText(tr("프로필 이름"));
        profileForm->addRow(tr("이름"), m_nameEdit);

        m_defaultProfileCheck = new QCheckBox(tr("기본 프로필로 지정"), profileGroup);
        m_defaultProfileCheck->setToolTip(
            tr("기본 프로필은 하나만 유지되며, 다음 실행부터 이 프로필이 먼저 열립니다. "
               "타겟은 지정할 수 없고 타겟 미지정 상태에서도 동작합니다."));
        profileForm->addRow(QString(), m_defaultProfileCheck);
        layout->addWidget(profileGroup);

        connect(m_defaultProfileCheck, &QCheckBox::toggled, this, &ProfileEditDialog::updateDefaultProfileUi);
    }

    m_linkedProgramSection = new QGroupBox(tr("대상 창"), this);
    m_linkedProgramSection->setToolTip(
        tr("프로필마다 메인·서브 창 제목을 저장합니다. 포커스에 따라 자동 전환됩니다."));
    auto* linkedLayout = new QVBoxLayout(m_linkedProgramSection);
    linkedLayout->setSpacing(8);

    auto* mainCaption = new QLabel(tr("메인 창"), m_linkedProgramSection);
    mainCaption->setStyleSheet(QStringLiteral("font-weight: 600;"));
    linkedLayout->addWidget(mainCaption);

    m_targetWindowTitleEdit = new QLineEdit(m_linkedProgramSection);
    m_targetWindowTitleEdit->setPlaceholderText(tr("타겟 제목 또는 일부 문자열"));
    m_targetWindowTitleEdit->setToolTip(
        tr("기능 실행 시 기본으로 이 제목을 기준으로 타겟을 찾습니다. 비워두면 타겟 미지정 상태로 저장됩니다."));
    linkedLayout->addWidget(m_targetWindowTitleEdit);

    auto* targetButtons = new QHBoxLayout();
    auto* useCurrentButton = new QPushButton(tr("현재 설정 사용"), m_linkedProgramSection);
    auto* pickTargetButton = new QPushButton(tr("지정"), m_linkedProgramSection);
    pickTargetButton->setToolTip(
        tr("클릭한 뒤 메인 창을 눌러 지정합니다. 마우스를 올리면 초록색 테두리가 표시됩니다."));
    auto* pickFromListButton = new QPushButton(tr("메인 목록"), m_linkedProgramSection);
    pickFromListButton->setToolTip(
        tr("메인 창 목록에서 선택합니다. 항목을 고르면 초록색 테두리 애니메이션이 표시됩니다."));
    auto* clearButton = new QPushButton(tr("비우기"), m_linkedProgramSection);
    targetButtons->addWidget(useCurrentButton);
    targetButtons->addWidget(pickTargetButton);
    targetButtons->addWidget(pickFromListButton);
    targetButtons->addWidget(clearButton);
    linkedLayout->addLayout(targetButtons);

    connect(useCurrentButton, &QPushButton::clicked, this, [this, currentTargetWindowTitle]() {
        if (m_targetWindowTitleEdit) {
            m_targetWindowTitleEdit->setText(currentTargetWindowTitle);
        }
    });
    connect(pickTargetButton, &QPushButton::clicked, this, [this]() {
        pickTargetWindowByClick(m_targetWindowTitleEdit, TargetWindowBindingRole::Main);
    });
    connect(pickFromListButton, &QPushButton::clicked, this, [this]() {
        openWindowListPicker(m_targetWindowTitleEdit, false);
    });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        if (m_targetWindowTitleEdit) {
            m_targetWindowTitleEdit->clear();
        }
    });

    auto* subCaption = new QLabel(tr("서브 창"), m_linkedProgramSection);
    subCaption->setStyleSheet(QStringLiteral("font-weight: 600;"));
    linkedLayout->addWidget(subCaption);

    m_subTargetWindowTitleEdit = new QLineEdit(m_linkedProgramSection);
    m_subTargetWindowTitleEdit->setPlaceholderText(tr("예: 런처 창 제목 또는 일부 문자열"));
    m_subTargetWindowTitleEdit->setToolTip(
        tr("같은 프로필로 자동 전환되는 추가 감지 창입니다. 이 창이 포커스일 때 기능을 실행하면 서브 타겟이 적용됩니다."));
    linkedLayout->addWidget(m_subTargetWindowTitleEdit);

    auto* subButtons = new QHBoxLayout();
    auto* subPickButton = new QPushButton(tr("지정"), m_linkedProgramSection);
    subPickButton->setToolTip(
        tr("클릭한 뒤 서브 창을 눌러 지정합니다. 마우스를 올리면 파란색 테두리가 표시됩니다."));
    auto* subPickListButton = new QPushButton(tr("서브 목록"), m_linkedProgramSection);
    subPickListButton->setToolTip(
        tr("서브 창 목록에서 선택합니다. 항목을 고르면 파란색 테두리 애니메이션이 표시됩니다."));
    auto* subClearButton = new QPushButton(tr("비우기"), m_linkedProgramSection);
    subButtons->addWidget(subPickButton);
    subButtons->addWidget(subPickListButton);
    subButtons->addWidget(subClearButton);
    subButtons->addStretch(1);
    linkedLayout->addLayout(subButtons);

    connect(subPickButton, &QPushButton::clicked, this, [this]() {
        pickTargetWindowByClick(m_subTargetWindowTitleEdit, TargetWindowBindingRole::Sub);
    });
    connect(subPickListButton, &QPushButton::clicked, this, [this]() {
        openWindowListPicker(m_subTargetWindowTitleEdit, true);
    });
    connect(subClearButton, &QPushButton::clicked, this, [this]() {
        if (m_subTargetWindowTitleEdit) {
            m_subTargetWindowTitleEdit->clear();
        }
    });

    layout->addWidget(m_linkedProgramSection);

    auto* buttons = new QDialogButtonBox(
        m_fixedDefaultProfile ? QDialogButtonBox::Ok : (QDialogButtonBox::Ok | QDialogButtonBox::Cancel), this);
    localizeDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProfileEditDialog::tryAccept);
    if (!m_fixedDefaultProfile) {
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    layout->addWidget(buttons);
}

void ProfileEditDialog::openWindowListPicker(QLineEdit* targetEdit, bool subTarget) {
#ifdef _WIN32
    if (!targetEdit) {
        return;
    }

    TargetWindowListPickOptions options;
    options.role = subTarget ? TargetWindowBindingRole::Sub : TargetWindowBindingRole::Main;
    options.mainBinding = m_targetWindowTitleEdit ? m_targetWindowTitleEdit->text().trimmed() : QString();
    options.subBinding =
        m_subTargetWindowTitleEdit ? m_subTargetWindowTitleEdit->text().trimmed() : QString();

    const std::optional<TargetWindowListPickResult> pickResult = ::pickTargetWindowFromList(this, options);
    if (!pickResult) {
        return;
    }

    targetEdit->setText(pickResult->title);
    if (pickResult->hwnd && IsWindow(pickResult->hwnd)) {
        const HWND selectedHwnd = pickResult->hwnd;
        const TargetWindowBindingRole role = options.role;
        QTimer::singleShot(0, this, [this, selectedHwnd, role]() {
            TargetWindowHighlightOverlay::flashSelectionWaveForHwnd(selectedHwnd, this, role);
        });
    }
#else
    Q_UNUSED(targetEdit);
    Q_UNUSED(subTarget);
#endif
}

void ProfileEditDialog::pickTargetWindowByClick(QLineEdit* targetEdit, TargetWindowBindingRole role) {
#ifdef _WIN32
    if (!targetEdit) {
        return;
    }
    WindowPicker::startPick(
        this,
        [this, targetEdit, role](const WindowPicker::Result& result) {
            if (!result.accepted || !result.hwnd) {
                return;
            }
            targetEdit->setText(QString::fromStdWString(result.title));
            TargetWindowHighlightOverlay::flashSelectionWaveForHwnd(result.hwnd, this, role);
        },
        role);
#else
    Q_UNUSED(targetEdit);
    Q_UNUSED(role);
#endif
}

void ProfileEditDialog::tryAccept() {
    if (m_fixedDefaultProfile) {
        accept();
        return;
    }
    if (!m_nameEdit || m_nameEdit->text().trimmed().isEmpty()) {
        m_nameEdit->setFocus(Qt::OtherFocusReason);
        return;
    }
    accept();
}

void ProfileEditDialog::updateDefaultProfileUi() {
    const bool isDefault = m_fixedDefaultProfile
                           || (m_defaultProfileCheck && m_defaultProfileCheck->isChecked());
    if (m_linkedProgramSection) {
        m_linkedProgramSection->setVisible(!isDefault);
    }
    if (isDefault) {
        if (m_targetWindowTitleEdit) {
            m_targetWindowTitleEdit->clear();
        }
        if (m_subTargetWindowTitleEdit) {
            m_subTargetWindowTitleEdit->clear();
        }
    }
}
