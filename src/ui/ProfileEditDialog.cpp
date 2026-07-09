#include "ui/ProfileEditDialog.h"

#include "core/capture/ScreenCapture.h"
#include "ui/UiStrings.h"
#include "ui/widgets/HintLabel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
struct ProfileWindowListEntry {
    HWND hwnd = nullptr;
    std::wstring title;
    QString displayText;
    QIcon icon;
};

QIcon profileIconForProcessPath(const std::wstring& processPath) {
    static QFileIconProvider iconProvider;
    if (!processPath.empty()) {
        const QIcon icon = iconProvider.icon(QFileInfo(QString::fromStdWString(processPath)));
        if (!icon.isNull()) {
            return icon;
        }
    }
    return iconProvider.icon(QFileIconProvider::File);
}

QVector<ProfileWindowListEntry> collectProfileWindowListEntries() {
    QVector<ProfileWindowListEntry> entries;
    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr) {
                return TRUE;
            }

            wchar_t titleBuffer[512]{};
            GetWindowTextW(hwnd, titleBuffer, 512);
            std::wstring title(titleBuffer);
            if (title.empty()) {
                return TRUE;
            }

            ScreenCapture::TargetWindowInfo info;
            QString processName = QStringLiteral("Unknown");
            std::wstring processPath;
            if (ScreenCapture::queryWindowInfo(hwnd, info) && !info.processPath.empty()) {
                processPath = info.processPath;
                processName = QFileInfo(QString::fromStdWString(processPath)).fileName();
            }

            auto* out = reinterpret_cast<QVector<ProfileWindowListEntry>*>(lParam);
            ProfileWindowListEntry entry;
            entry.hwnd = hwnd;
            entry.title = title;
            entry.icon = profileIconForProcessPath(processPath);
            entry.displayText = QStringLiteral("%1 - %2")
                                    .arg(processName, QString::fromStdWString(title));
            out->push_back(std::move(entry));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&entries));
    return entries;
}
#endif

} // namespace

ProfileEditDialog::ProfileEditDialog(const QString& profileName,
                                     const QString& targetWindowTitle,
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
        return out;
    }
    out.name = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
    out.defaultProfile = m_defaultProfileCheck && m_defaultProfileCheck->isChecked();
    out.targetWindowTitle =
        out.defaultProfile || !m_targetWindowTitleEdit ? QString() : m_targetWindowTitleEdit->text().trimmed();
    return out;
}

void ProfileEditDialog::setupUi(const QString& currentTargetWindowTitle) {
    auto* layout = new QVBoxLayout(this);

    if (m_fixedDefaultProfile) {
        auto* hint = new HintLabel(
            tr("시스템 기본 프로필입니다. 이름·대상 창·프로필 순서는 변경할 수 없으며, 목록 맨 위에 고정됩니다."),
            this);
        layout->addWidget(hint);

        auto* nameLabel = new QLabel(tr("프로필 이름"), this);
        layout->addWidget(nameLabel);

        m_fixedDefaultNameLabel = new QLabel(QStringLiteral("기본"), this);
        m_fixedDefaultNameLabel->setObjectName(QStringLiteral("fixedDefaultProfileName"));
        m_fixedDefaultNameLabel->setStyleSheet(QStringLiteral(
            "QLabel#fixedDefaultProfileName {"
            "  font-weight: 600;"
            "  padding: 6px 10px;"
            "  border-radius: 6px;"
            "  background-color: palette(button);"
            "  color: palette(windowText);"
            "}"));
        layout->addWidget(m_fixedDefaultNameLabel);

        auto* lockedHint = new HintLabel(
            tr("전역으로 동작하며 대상 창을 지정하지 않습니다. 연결된 프로그램이 없을 때 자동으로 선택됩니다."),
            this);
        layout->addWidget(lockedHint);
    } else {
        auto* hint = new HintLabel(
            tr("프로필마다 이름과 연결된 프로그램(대상 창 제목)을 따로 저장합니다. 기본 프로필은 대상 창 없이 전역으로 동작합니다."),
            this);
        layout->addWidget(hint);

        auto* nameLabel = new QLabel(tr("프로필 이름"), this);
        layout->addWidget(nameLabel);

        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setPlaceholderText(tr("프로필 이름"));
        layout->addWidget(m_nameEdit);
    }

    m_linkedProgramSection = new QWidget(this);
    auto* linkedLayout = new QVBoxLayout(m_linkedProgramSection);
    linkedLayout->setContentsMargins(0, 0, 0, 0);
    linkedLayout->setSpacing(6);

    auto* targetLabel = new QLabel(tr("연결된 프로그램"), m_linkedProgramSection);
    linkedLayout->addWidget(targetLabel);

    m_targetWindowTitleEdit = new QLineEdit(m_linkedProgramSection);
    m_targetWindowTitleEdit->setPlaceholderText(tr("대상 창 제목 또는 일부 문자열"));
    linkedLayout->addWidget(m_targetWindowTitleEdit);

    auto* targetHint = new HintLabel(
        tr("기능 실행 시 이 제목을 기준으로 대상 창을 찾습니다. 비워두면 창 미지정 상태로 저장됩니다."),
        m_linkedProgramSection);
    linkedLayout->addWidget(targetHint);

    auto* targetButtons = new QHBoxLayout();
    auto* useCurrentButton = new QPushButton(tr("현재 설정 사용"), m_linkedProgramSection);
    auto* pickFromListButton = new QPushButton(tr("창 목록"), m_linkedProgramSection);
    auto* clearButton = new QPushButton(tr("비우기"), m_linkedProgramSection);
    targetButtons->addWidget(useCurrentButton);
    targetButtons->addWidget(pickFromListButton);
    targetButtons->addWidget(clearButton);
    linkedLayout->addLayout(targetButtons);

    connect(useCurrentButton, &QPushButton::clicked, this, [this, currentTargetWindowTitle]() {
        if (m_targetWindowTitleEdit) {
            m_targetWindowTitleEdit->setText(currentTargetWindowTitle);
        }
    });
    connect(pickFromListButton, &QPushButton::clicked, this, &ProfileEditDialog::openWindowListPicker);
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        if (m_targetWindowTitleEdit) {
            m_targetWindowTitleEdit->clear();
        }
    });

    layout->addWidget(m_linkedProgramSection);

    if (!m_fixedDefaultProfile) {
        m_defaultProfileCheck = new QCheckBox(tr("기본 프로필로 지정"), this);
        layout->addWidget(m_defaultProfileCheck);

        auto* defaultHint = new HintLabel(
            tr("기본 프로필은 하나만 유지되며, 다음 실행부터 이 프로필이 먼저 열립니다. 대상 창은 지정할 수 없고 창 미지정 상태에서도 동작합니다."),
            this);
        layout->addWidget(defaultHint);

        connect(m_defaultProfileCheck, &QCheckBox::toggled, this, &ProfileEditDialog::updateDefaultProfileUi);
    }

    auto* buttons = new QDialogButtonBox(
        m_fixedDefaultProfile ? QDialogButtonBox::Ok : (QDialogButtonBox::Ok | QDialogButtonBox::Cancel), this);
    localizeDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProfileEditDialog::tryAccept);
    if (!m_fixedDefaultProfile) {
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    layout->addWidget(buttons);
}

void ProfileEditDialog::openWindowListPicker() {
#ifdef _WIN32
    QDialog dialog(this);
    dialog.setWindowTitle(tr("창 목록"));
    dialog.resize(700, 420);

    auto* layout = new QVBoxLayout(&dialog);
    auto* hint = new HintLabel(tr("현재 표시 중인 창에서 프로필에 연결할 프로그램을 고르세요."), &dialog);
    layout->addWidget(hint);

    auto* listWidget = new QListWidget(&dialog);
    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget->setIconSize(QSize(24, 24));
    layout->addWidget(listWidget, 1);

    const QVector<ProfileWindowListEntry> entries = collectProfileWindowListEntries();
    for (const ProfileWindowListEntry& entry : entries) {
        auto* item = new QListWidgetItem(entry.icon, entry.displayText, listWidget);
        item->setData(Qt::UserRole, QString::fromStdWString(entry.title));
    }
    if (listWidget->count() > 0) {
        listWidget->setCurrentRow(0);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    localizeDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (listWidget->count() == 0) {
        return;
    }
    if (dialog.exec() != QDialog::Accepted || !listWidget->currentItem()) {
        return;
    }
    m_targetWindowTitleEdit->setText(listWidget->currentItem()->data(Qt::UserRole).toString());
#else
    (void)this;
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
    if (isDefault && m_targetWindowTitleEdit) {
        m_targetWindowTitleEdit->clear();
    }
}
