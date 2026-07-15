#include "ui/memo/MemoDialog.h"

#include "storage/ProfileMemoStore.h"

#include <QCloseEvent>
#include <QFont>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QShowEvent>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString memoOpenSettingsKey(const QString& profileId) {
    return QStringLiteral("memo/open/%1").arg(profileId);
}

constexpr int kShadowSize = 8;
constexpr int kNoteRadius = 4;

} // namespace

MemoDialog::MemoDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("메모장"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(300, 240);
    resize(420, 340);

    m_profileLabel = new QLabel(this);
    m_profileLabel->setTextFormat(Qt::PlainText);
    QFont profileFont = m_profileLabel->font();
    profileFont.setPointSize(9);
    profileFont.setBold(true);
    m_profileLabel->setFont(profileFont);

    m_closeButton = new QPushButton(QStringLiteral("×"), this);
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setFlat(true);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setToolTip(tr("닫기"));
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);

    m_editor = new QTextEdit(this);
    m_editor->setAcceptRichText(false);
    m_editor->setFrameShape(QFrame::NoFrame);
    m_editor->setPlaceholderText(tr("메모를 입력하세요…"));
    QFont editorFont = m_editor->font();
    editorFont.setPointSize(11);
    m_editor->setFont(editorFont);
    m_editor->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background: transparent;"
        "  border: none;"
        "  color: #4a4435;"
        "  selection-background-color: #ffe082;"
        "  selection-color: #3a3428;"
        "}"
        "QTextEdit::placeholder {"
        "  color: #9a8f6e;"
        "}"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14 + kShadowSize, 12 + kShadowSize);
    layout->setSpacing(4);
    layout->addWidget(m_profileLabel);
    layout->addWidget(m_editor, 1);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(600);
    connect(m_saveTimer, &QTimer::timeout, this, &MemoDialog::saveNow);
    connect(m_editor, &QTextEdit::textChanged, this, &MemoDialog::scheduleSave);

    updateChrome();
}

void MemoDialog::setProfile(const QString& profileId,
                            const QString& profileDirectory,
                            const QString& profileDisplayName) {
    if (profileId == m_profileId && profileDirectory == m_profileDirectory) {
        m_profileDisplayName = profileDisplayName;
        updateWindowTitle();
        updateChrome();
        return;
    }

    saveNow();

    m_profileId = profileId;
    m_profileDirectory = profileDirectory;
    m_profileDisplayName = profileDisplayName;
    updateWindowTitle();
    updateChrome();

    m_loading = true;
    QString text;
    ProfileMemoStore::load(profileDirectory, &text);
    m_editor->setPlainText(text);
    m_loading = false;
}

void MemoDialog::saveNow() {
    if (m_loading || m_profileDirectory.isEmpty()) {
        return;
    }
    m_saveTimer->stop();
    ProfileMemoStore::save(m_profileDirectory, m_editor->toPlainText());
}

void MemoDialog::scheduleSave() {
    if (m_loading || m_profileDirectory.isEmpty()) {
        return;
    }
    m_saveTimer->start();
}

void MemoDialog::closeEvent(QCloseEvent* event) {
    saveNow();
    persistGeometry();
    if (event->spontaneous() && !m_profileId.isEmpty()) {
        QSettings settings;
        settings.setValue(memoOpenSettingsKey(m_profileId), false);
    }
    QDialog::closeEvent(event);
}

void MemoDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    restorePersistedGeometry();
    if (!m_profileId.isEmpty()) {
        QSettings settings;
        settings.setValue(memoOpenSettingsKey(m_profileId), true);
    }
}

void MemoDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    const QRect full = rect();
    const QRect noteRect = full.adjusted(0, 0, -kShadowSize, -kShadowSize);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 42));
    painter.drawRoundedRect(noteRect.translated(kShadowSize, kShadowSize), kNoteRadius, kNoteRadius);

    QLinearGradient paperGradient(noteRect.topLeft(), noteRect.bottomLeft());
    paperGradient.setColorAt(0.0, QColor(0xff, 0xf9, 0xc0));
    paperGradient.setColorAt(1.0, QColor(0xff, 0xf3, 0x9a));
    painter.setBrush(paperGradient);
    painter.setPen(QPen(QColor(0xe6, 0xd7, 0x78), 1));
    painter.drawRoundedRect(noteRect, kNoteRadius, kNoteRadius);

    const int fold = 18;
    const QPoint foldTopRight(noteRect.right(), noteRect.top());
    QPolygon foldTriangle;
    foldTriangle << foldTopRight << QPoint(foldTopRight.x() - fold, foldTopRight.y())
                 << QPoint(foldTopRight.x(), foldTopRight.y() + fold);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xf0, 0xe2, 0x88));
    painter.drawPolygon(foldTriangle);

    painter.setPen(QPen(QColor(0xd8, 0xc8, 0x70), 1));
    painter.drawLine(foldTopRight.x() - fold, foldTopRight.y(),
                     foldTopRight.x(), foldTopRight.y() + fold);

    const int ruleY = noteRect.top() + m_headerHeight;
    painter.setPen(QPen(QColor(0xeb, 0xdf, 0x9a), 1));
    painter.drawLine(noteRect.left() + 10, ruleY, noteRect.right() - 10, ruleY);
}

void MemoDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    if (m_closeButton) {
        const QRect noteRect = rect().adjusted(0, 0, -kShadowSize, -kShadowSize);
        m_closeButton->move(noteRect.right() - m_closeButton->width() - 6, noteRect.top() + 6);
    }
}

bool MemoDialog::isInDragRegion(const QPoint& pos) const {
    const QRect noteRect = rect().adjusted(0, 0, -kShadowSize, -kShadowSize);
    if (!noteRect.contains(pos)) {
        return false;
    }
    const QRect headerRect(noteRect.left(), noteRect.top(), noteRect.width(), m_headerHeight);
    if (!headerRect.contains(pos)) {
        return false;
    }
    if (m_closeButton && m_closeButton->geometry().contains(pos)) {
        return false;
    }
    return true;
}

void MemoDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && isInDragRegion(event->pos())) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void MemoDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    if (isInDragRegion(event->pos())) {
        setCursor(Qt::SizeAllCursor);
    } else {
        unsetCursor();
    }
    QDialog::mouseMoveEvent(event);
}

void MemoDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
    QDialog::mouseReleaseEvent(event);
}

void MemoDialog::persistGeometry() {
    QSettings settings;
    settings.setValue(QStringLiteral("memo/geometry"), saveGeometry());
}

void MemoDialog::restorePersistedGeometry() {
    const QByteArray geometry =
        QSettings().value(QStringLiteral("memo/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        QDialog::restoreGeometry(geometry);
    }
}

void MemoDialog::updateWindowTitle() {
    if (m_profileDisplayName.isEmpty()) {
        setWindowTitle(tr("메모장"));
        return;
    }
    setWindowTitle(tr("메모장 — %1").arg(m_profileDisplayName));
}

void MemoDialog::updateChrome() {
    if (!m_profileLabel) {
        return;
    }
    if (m_profileDisplayName.isEmpty()) {
        m_profileLabel->setText(tr("메모"));
    } else {
        m_profileLabel->setText(m_profileDisplayName);
    }
    m_profileLabel->setStyleSheet(QStringLiteral(
        "color: #7a6f52; background: transparent; padding-right: 28px;"));

    if (m_closeButton) {
        m_closeButton->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  color: #8a7d5c;"
            "  background: transparent;"
            "  border: none;"
            "  font-size: 16px;"
            "  font-weight: bold;"
            "  border-radius: 12px;"
            "}"
            "QPushButton:hover {"
            "  color: #4a4435;"
            "  background: rgba(0, 0, 0, 12);"
            "}"));
    }
}
