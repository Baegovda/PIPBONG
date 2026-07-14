#include "ui/AppHelpDialog.h"

#include "PipbongVersion.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

QString helpSectionHtml(const QString& title, const QStringList& bullets) {
    QString html = QStringLiteral("<p style='margin-top:14px; margin-bottom:6px;'>"
                                    "<b>%1</b></p><ul style='margin-top:0; margin-bottom:0;'>")
                         .arg(title.toHtmlEscaped());
    for (const QString& bullet : bullets) {
        html += QStringLiteral("<li style='margin-bottom:4px;'>%1</li>").arg(bullet.toHtmlEscaped());
    }
    html += QStringLiteral("</ul>");
    return html;
}

QString pipbongHelpHtml() {
    QString html = QStringLiteral(
        "<body style='font-size:10pt; line-height:1.45;'>"
        "<p style='margin-top:0;'>%1</p>")
                       .arg(QCoreApplication::translate(
                                "AppHelpDialog",
                                "PIPBONG은 블록으로 조립하는 Windows 화면 자동화 도구입니다. "
                                "대상 창을 지정하고 워크플로우를 만들어 단축키로 실행합니다.")
                                .toHtmlEscaped());

    html += helpSectionHtml(
        QCoreApplication::translate("AppHelpDialog", "빠른 시작"),
        {QCoreApplication::translate("AppHelpDialog",
                                    "창 지정 또는 창 목록으로 자동화할 프로그램 창을 고릅니다."),
         QCoreApplication::translate("AppHelpDialog", "왼쪽에서 기능을 추가하고 오른쪽에 블록을 쌓습니다."),
         QCoreApplication::translate("AppHelpDialog",
                                    "각 블록을 편집합니다(템플릿 캡처, 좌표, 키, 딜레이 등)."),
         QCoreApplication::translate("AppHelpDialog",
                                    "기능 편집에서 기능마다 전역 단축키를 지정할 수 있습니다."),
         QCoreApplication::translate("AppHelpDialog", "기능 목록의 실행 버튼이나 단축키로 워크플로우를 시작합니다."),
         QCoreApplication::translate("AppHelpDialog",
                                    "변경 사항은 자동 저장됩니다. 파일 메뉴에서 프로젝트를 저장하거나 열 수 있습니다.")});

    html += helpSectionHtml(
        QCoreApplication::translate("AppHelpDialog", "블록 종류"),
        {QCoreApplication::translate("AppHelpDialog",
                                    "템플릿 매칭 — 화면에서 이미지를 찾고, 이후 클릭의 기준 위치로 씁니다."),
         QCoreApplication::translate("AppHelpDialog",
                                    "마우스 — 고정 좌표, 직전 매칭, 현재 커서 위치에서 클릭·누름·이동."),
         QCoreApplication::translate("AppHelpDialog", "키보드 — 키 탭, 누름, 뗌 및 조합키 동작."),
         QCoreApplication::translate("AppHelpDialog", "딜레이 — 고정 또는 랜덤 시간만큼 대기."),
         QCoreApplication::translate("AppHelpDialog", "텍스트 — 설정한 문자열을 대상 앱에 입력."),
         QCoreApplication::translate("AppHelpDialog",
                                    "구간 반복 — 블록 범위를 조건에 맞을 때까지 반복(워크플로우 도구 모음).")});

    html += helpSectionHtml(
        QCoreApplication::translate("AppHelpDialog", "참고"),
        {QCoreApplication::translate("AppHelpDialog",
                                    "템플릿 매칭 편집기의 화면에서 캡처로 대상 창 위 영역을 드래그해 "
                                    "템플릿을 만듭니다."),
         QCoreApplication::translate("AppHelpDialog", "매칭 테스트로 탐지 결과를 대상 창 위에 미리 볼 수 있습니다."),
         QCoreApplication::translate("AppHelpDialog",
                                    "대상 프로그램이 관리자 권한으로 실행 중이면 PIPBONG도 같은 권한으로 "
                                    "실행해야 입력이 전달될 수 있습니다."),
         QCoreApplication::translate("AppHelpDialog", "하단 업데이트 버튼으로 GitHub 최신 버전을 확인합니다."),
         QCoreApplication::translate("AppHelpDialog",
                                    "계산기는 poe.ninja 시세 표(워크플로우와 별도)를 엽니다.")});

    const QString repoUrl =
        QStringLiteral("https://github.com/%1").arg(QStringLiteral(PIPBONG_UPDATE_GITHUB_REPO));
    html += QStringLiteral("<p style='margin-top:14px;'>%1 "
                           "<a href='%2'>%2</a></p>")
                .arg(QCoreApplication::translate("AppHelpDialog", "자세한 내용 및 다운로드:").toHtmlEscaped(),
                     repoUrl.toHtmlEscaped());

    html += QStringLiteral("</body>");
    return html;
}

void showTextDialog(QWidget* parent, const QString& title, const QString& html, const QSize& size) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.resize(size);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* browser = new QTextBrowser(&dialog);
    browser->setReadOnly(true);
    browser->setOpenExternalLinks(true);
    browser->setHtml(html);
    layout->addWidget(browser, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

} // namespace

void AppHelpDialog::showHelp(QWidget* parent) {
    showTextDialog(parent,
                   QCoreApplication::translate("AppHelpDialog", "PIPBONG 도움말"),
                   pipbongHelpHtml(),
                   QSize(560, 540));
}

void AppHelpDialog::showAbout(QWidget* parent) {
    const QString version = QCoreApplication::applicationVersion();
    const QString repoUrl =
        QStringLiteral("https://github.com/%1").arg(QStringLiteral(PIPBONG_UPDATE_GITHUB_REPO));
    const QString releasesUrl = repoUrl + QStringLiteral("/releases/latest");

    QDialog dialog(parent);
    dialog.setWindowTitle(QCoreApplication::translate("AppHelpDialog", "PIPBONG 정보"));
    dialog.setModal(true);
    dialog.setFixedWidth(420);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(10);

    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(14);

    auto* iconLabel = new QLabel(&dialog);
    const QIcon icon = QApplication::windowIcon();
    if (!icon.isNull()) {
        iconLabel->setPixmap(icon.pixmap(64, 64));
    }
    iconLabel->setAlignment(Qt::AlignTop);
    headerRow->addWidget(iconLabel);

    auto* titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(4);

    auto* nameLabel = new QLabel(QStringLiteral("PIPBONG"), &dialog);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(nameFont.pointSize() + 4);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    titleColumn->addWidget(nameLabel);

    auto* versionLabel = new QLabel(
        QCoreApplication::translate("AppHelpDialog", "버전 %1").arg(version), &dialog);
    titleColumn->addWidget(versionLabel);

    auto* taglineLabel = new QLabel(
        QCoreApplication::translate("AppHelpDialog", "블록 기반 Windows 화면 자동화 도구"), &dialog);
    taglineLabel->setWordWrap(true);
    titleColumn->addWidget(taglineLabel);
    titleColumn->addStretch();
    headerRow->addLayout(titleColumn, 1);
    layout->addLayout(headerRow);

    auto* browser = new QTextBrowser(&dialog);
    browser->setReadOnly(true);
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    const QString aboutHtml = QStringLiteral(
                                  "<body style='font-size:9.5pt; line-height:1.45; margin:0;'>"
                                  "<p>%1</p>"
                                  "<p>%2</p>"
                                  "<p><a href='%3'>%3</a></p>"
                                  "</body>")
                                  .arg(QCoreApplication::translate(
                                           "AppHelpDialog",
                                           "템플릿 매칭, 마우스·키보드 입력, 딜레이, 구간 반복으로 "
                                           "반복 작업을 자동화합니다.")
                                           .toHtmlEscaped(),
                                       QCoreApplication::translate(
                                           "AppHelpDialog",
                                           "주의: 자동화는 대상 프로그램의 이용 약관을 위반할 수 있습니다. "
                                           "사용자 책임 하에 이용하세요.")
                                           .toHtmlEscaped(),
                                       releasesUrl.toHtmlEscaped());
    browser->setHtml(aboutHtml);
    browser->document()->setTextWidth(browser->viewport()->width());
    const int docHeight = static_cast<int>(browser->document()->size().height()) + 8;
    browser->setFixedHeight(qMax(72, docHeight));
    layout->addWidget(browser);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}
