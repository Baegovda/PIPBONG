#include "ui/TargetWindowListPicker.h"

#include "core/capture/ScreenCapture.h"
#include "ui/TargetWindowBindingRole.h"
#include "ui/UiThemeColors.h"
#include "ui/WindowPickerHoverOverlay.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#ifdef _WIN32
#include <windows.h>
#endif

bool targetWindowTitleMatchesMainBinding(const QString& windowTitle, const QString& mainBinding) {
    return !mainBinding.isEmpty() && !windowTitle.isEmpty()
           && windowTitle.contains(mainBinding, Qt::CaseInsensitive);
}

bool targetWindowTitleMatchesSubTarget(const QString& windowTitle,
                                       const QString& mainBinding,
                                       const QString& subBinding) {
    if (subBinding.isEmpty() || windowTitle.isEmpty()) {
        return false;
    }
    const bool subHit = windowTitle.contains(subBinding, Qt::CaseInsensitive);
    const bool mainHit =
        !mainBinding.isEmpty() && windowTitle.contains(mainBinding, Qt::CaseInsensitive);
    return subHit && (!mainHit || subBinding.length() >= mainBinding.length());
}

void applyTargetWindowListBindingMark(QListWidgetItem* item,
                                      const QPalette& pal,
                                      bool isMainBinding,
                                      bool isSubBinding) {
    if (!item || (!isMainBinding && !isSubBinding)) {
        return;
    }

    const bool dark = isDarkTheme(pal);
    const QColor mainAccent = targetWindowBindingAccentColor(TargetWindowBindingRole::Main, dark);
    const QColor subAccent = targetWindowBindingAccentColor(TargetWindowBindingRole::Sub, dark);

    QColor tint = isMainBinding ? mainAccent : subAccent;
    if (isMainBinding && isSubBinding) {
        tint = QColor((mainAccent.red() + subAccent.red()) / 2,
                      (mainAccent.green() + subAccent.green()) / 2,
                      (mainAccent.blue() + subAccent.blue()) / 2);
    }
    tint.setAlpha(dark ? 48 : 40);

    QString badges;
    if (isMainBinding) {
        badges += QStringLiteral("● 메인 창");
    }
    if (isSubBinding) {
        if (!badges.isEmpty()) {
            badges += QStringLiteral("  ");
        }
        badges += QStringLiteral("● 서브 창");
    }

    item->setText(item->text() + QStringLiteral("    ") + badges);
    item->setToolTip(badges);
    item->setBackground(QBrush(tint));

    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);

    const QColor accent =
        isMainBinding && isSubBinding ? primaryContentTextColor(pal)
        : isMainBinding                  ? mainAccent
                                         : subAccent;
    item->setForeground(QBrush(accent));
}

QString targetWindowListBindingLegendHtml(const QPalette& pal) {
    const bool dark = isDarkTheme(pal);
    const QColor mainAccent = targetWindowBindingAccentColor(TargetWindowBindingRole::Main, dark);
    const QColor subAccent = targetWindowBindingAccentColor(TargetWindowBindingRole::Sub, dark);
    return QStringLiteral(
               "<span style='color:%1; font-weight:600'>● 메인 창</span>"
               "&nbsp;&nbsp;"
               "<span style='color:%2; font-weight:600'>● 서브 창</span>")
        .arg(mainAccent.name(), subAccent.name());
}

#ifdef _WIN32

namespace {

void wireTargetWindowListHoverFeedback(QListWidget* listWidget, TargetWindowBindingRole role) {
    if (!listWidget) {
        return;
    }
    listWidget->setMouseTracking(true);
    const auto applyHover = [listWidget, role]() {
        QListWidgetItem* current = listWidget->currentItem();
        if (!current) {
            WindowPickerHoverOverlay::dismissAll();
            return;
        }
        const HWND hwnd = reinterpret_cast<HWND>(current->data(Qt::UserRole).toULongLong());
        if (hwnd && IsWindow(hwnd)) {
            WindowPickerHoverOverlay::updateHover(hwnd, role);
        } else {
            WindowPickerHoverOverlay::dismissAll();
        }
    };
    QObject::connect(listWidget, &QListWidget::currentItemChanged, listWidget,
                     [applyHover](QListWidgetItem*, QListWidgetItem*) { applyHover(); });
    QObject::connect(listWidget, &QListWidget::itemEntered, listWidget,
                     [listWidget, applyHover](QListWidgetItem* item) {
                         if (item) {
                             listWidget->setCurrentItem(item);
                         }
                         applyHover();
                     });
}

} // namespace

QIcon targetWindowIconForProcessPath(const std::wstring& processPath) {
    static QFileIconProvider iconProvider;
    if (!processPath.empty()) {
        const QIcon icon = iconProvider.icon(QFileInfo(QString::fromStdWString(processPath)));
        if (!icon.isNull()) {
            return icon;
        }
    }
    return iconProvider.icon(QFileIconProvider::File);
}

QVector<TargetWindowListEntry> collectTargetWindowListEntries() {
    QVector<TargetWindowListEntry> entries;
    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            if (!IsWindowVisible(hwnd)) {
                return TRUE;
            }
            if (GetWindow(hwnd, GW_OWNER) != nullptr) {
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
                const QFileInfo processInfo(QString::fromStdWString(processPath));
                processName = processInfo.fileName();
            }

            auto* out = reinterpret_cast<QVector<TargetWindowListEntry>*>(lParam);
            TargetWindowListEntry entry;
            entry.hwnd = hwnd;
            entry.title = title;
            entry.processPath = QString::fromStdWString(processPath);
            entry.icon = targetWindowIconForProcessPath(processPath);
            const QString titleText = QString::fromStdWString(title);
            const qulonglong hwndValue = reinterpret_cast<qulonglong>(hwnd);
            entry.displayText = QStringLiteral("[%1] %2 - %3")
                                    .arg(hwndValue, 0, 16)
                                    .toUpper()
                                    .arg(processName, titleText);
            out->push_back(std::move(entry));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&entries));
    return entries;
}

std::optional<TargetWindowListPickResult> pickTargetWindowFromList(QWidget* parent,
                                                                   const TargetWindowListPickOptions& options) {
    const bool pickSub = options.role == TargetWindowBindingRole::Sub;

    QDialog dialog(parent);
    dialog.setWindowTitle(pickSub ? QObject::tr("서브 창 목록") : QObject::tr("메인 창 목록"));
    dialog.resize(760, 460);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* hintLabel = new QLabel(
        pickSub ? QObject::tr("현재 데스크톱에 표시된 최상위 창 목록입니다. 서브 창으로 연결할 항목을 고르세요. "
                               "선택하면 해당 창에 파란색 테두리 애니메이션이 표시됩니다.")
                : QObject::tr("현재 데스크톱에 표시된 최상위 창 목록입니다. 메인 창으로 연결할 항목을 고르세요. "
                               "선택하면 해당 창에 초록색 테두리 애니메이션이 표시됩니다. "
                               "더블클릭하거나 선택 후 확인을 누르세요."),
        &dialog);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    auto* legendLabel = new QLabel(&dialog);
    legendLabel->setTextFormat(Qt::RichText);
    legendLabel->setText(targetWindowListBindingLegendHtml(dialog.palette()));
    layout->addWidget(legendLabel);

    auto* listWidget = new QListWidget(&dialog);
    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget->setIconSize(QSize(24, 24));
    layout->addWidget(listWidget, 1);

    auto* refreshButton = new QPushButton(QObject::tr("새로고침"), &dialog);
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addWidget(refreshButton);
    buttonRow->addStretch();

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* okButton = buttonBox->button(QDialogButtonBox::Ok);
    if (okButton) {
        okButton->setText(QObject::tr("선택"));
        okButton->setEnabled(false);
    }
    if (auto* cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(QObject::tr("취소"));
    }
    buttonRow->addWidget(buttonBox);
    layout->addLayout(buttonRow);

    const QString mainBinding = options.mainBinding.trimmed();
    const QString subBinding = options.subBinding.trimmed();
    const HWND preferredHwnd = options.preferredHwnd;

    auto populateList = [listWidget, mainBinding, subBinding, pickSub, preferredHwnd]() {
        const QVector<TargetWindowListEntry> entries = collectTargetWindowListEntries();
        const QPalette listPalette = listWidget->palette();
        listWidget->clear();

        int selectedIndex = -1;
        int subMatchIndex = -1;
        for (int i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            auto* item = new QListWidgetItem(entry.displayText, listWidget);
            item->setIcon(entry.icon);
            item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<qulonglong>(entry.hwnd)));
            item->setData(Qt::UserRole + 1, QString::fromStdWString(entry.title));

            const QString entryTitle = QString::fromStdWString(entry.title);
            const bool isMainBinding =
                targetWindowTitleMatchesMainBinding(entryTitle, mainBinding)
                || (preferredHwnd && preferredHwnd == entry.hwnd);
            const bool isSubBinding =
                targetWindowTitleMatchesSubTarget(entryTitle, mainBinding, subBinding);
            applyTargetWindowListBindingMark(item, listPalette, isMainBinding, isSubBinding);

            if (pickSub) {
                if (subMatchIndex < 0 && isSubBinding) {
                    subMatchIndex = i;
                }
            } else if (preferredHwnd && preferredHwnd == entry.hwnd) {
                selectedIndex = i;
            } else if (selectedIndex < 0 && isMainBinding) {
                selectedIndex = i;
            }
            if (subMatchIndex < 0 && isSubBinding) {
                subMatchIndex = i;
            }
        }

        if (pickSub) {
            if (subMatchIndex >= 0) {
                listWidget->setCurrentRow(subMatchIndex);
            } else if (listWidget->count() > 0) {
                listWidget->setCurrentRow(0);
            }
        } else if (selectedIndex >= 0) {
            listWidget->setCurrentRow(selectedIndex);
        } else if (listWidget->count() > 0) {
            listWidget->setCurrentRow(0);
        }
    };

    wireTargetWindowListHoverFeedback(listWidget, options.role);

    QObject::connect(refreshButton, &QPushButton::clicked, &dialog, populateList);
    QObject::connect(listWidget, &QListWidget::itemDoubleClicked, &dialog,
                     [&dialog](QListWidgetItem*) { dialog.accept(); });
    QObject::connect(listWidget, &QListWidget::currentItemChanged, &dialog,
                     [okButton](QListWidgetItem* current) {
                         if (okButton) {
                             okButton->setEnabled(current != nullptr);
                         }
                     });
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    struct ListPickerHoverGuard {
        ~ListPickerHoverGuard() { WindowPickerHoverOverlay::dismissAll(); }
    } hoverGuard;

    populateList();
    if (listWidget->count() == 0) {
        QMessageBox::information(parent,
                                 pickSub ? QObject::tr("서브 창 목록")
                                         : QObject::tr("메인 창 목록"),
                                 QObject::tr("표시 중인 창을 찾지 못했습니다."));
        return std::nullopt;
    }

    QTimer::singleShot(0, &dialog, [listWidget, role = options.role]() {
        if (QListWidgetItem* current = listWidget->currentItem()) {
            const HWND hwnd = reinterpret_cast<HWND>(current->data(Qt::UserRole).toULongLong());
            if (hwnd && IsWindow(hwnd)) {
                WindowPickerHoverOverlay::updateHover(hwnd, role);
            }
        }
    });

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    QListWidgetItem* currentItem = listWidget->currentItem();
    if (!currentItem) {
        return std::nullopt;
    }

    const HWND hwnd = reinterpret_cast<HWND>(currentItem->data(Qt::UserRole).toULongLong());
    if (!hwnd || !IsWindow(hwnd)) {
        QMessageBox::warning(parent,
                             pickSub ? QObject::tr("서브 창 목록") : QObject::tr("메인 창 목록"),
                             QObject::tr("선택한 창이 더 이상 유효하지 않습니다. 다시 선택하세요."));
        return std::nullopt;
    }

    TargetWindowListPickResult result;
    result.hwnd = hwnd;
    result.title = currentItem->data(Qt::UserRole + 1).toString();
    return result;
}

#endif
