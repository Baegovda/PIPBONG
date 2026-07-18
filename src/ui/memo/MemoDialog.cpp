#include "ui/memo/MemoDialog.h"

#include "storage/ProfileMemoStore.h"
#include "ui/UiThemeColors.h"

#include <QCloseEvent>
#include <QEvent>
#include <QFont>
#include <QGuiApplication>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QShowEvent>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace {

QString memoOpenSettingsKey(const QString& profileId) {
    return QStringLiteral("memo/open/%1").arg(profileId);
}

constexpr int kShadowSize = 8;
constexpr int kNoteRadius = 10;

QScreen* screenByName(const QString& screenName) {
    if (screenName.isEmpty()) {
        return nullptr;
    }
    for (QScreen* screen : QGuiApplication::screens()) {
        if (screen && screen->name() == screenName) {
            return screen;
        }
    }
    return nullptr;
}

QRect clampWindowRectToScreen(const QRect& rect, QScreen* screen) {
    if (!screen || !rect.isValid()) {
        return rect;
    }
    const QRect avail = screen->availableGeometry();
    QRect clamped = rect;
    if (clamped.width() > avail.width()) {
        clamped.setWidth(avail.width());
    }
    if (clamped.height() > avail.height()) {
        clamped.setHeight(avail.height());
    }
    if (clamped.right() > avail.right()) {
        clamped.moveRight(avail.right());
    }
    if (clamped.bottom() > avail.bottom()) {
        clamped.moveBottom(avail.bottom());
    }
    if (clamped.left() < avail.left()) {
        clamped.moveLeft(avail.left());
    }
    if (clamped.top() < avail.top()) {
        clamped.moveTop(avail.top());
    }
    return clamped;
}

void assignDialogToScreen(QWidget* window, QScreen* screen) {
    if (!window || !screen) {
        return;
    }
    if (QWindow* handle = window->windowHandle()) {
        handle->setScreen(screen);
    }
}

} // namespace

MemoDialog::MemoDialog(QWidget* parent)
    : QDialog(parent) {
    setProperty("pipbong_featureHotkeyGateExempt", true);
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
        "  color: palette(text);"
        "  selection-background-color: palette(highlight);"
        "  selection-color: palette(highlighted-text);"
        "}"
        "QTextEdit::placeholder {"
        "  color: palette(placeholderText);"
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

void MemoDialog::prepareForApplicationShutdown() {
    m_preserveOpenStateOnClose = isVisible();
}

void MemoDialog::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        updateChrome();
        update();
        break;
    default:
        break;
    }
}

void MemoDialog::closeEvent(QCloseEvent* event) {
    saveNow();
    persistPlacement();
    persistOpenState(m_preserveOpenStateOnClose);
    m_preserveOpenStateOnClose = false;
    QDialog::closeEvent(event);
}

void MemoDialog::hideEvent(QHideEvent* event) {
    persistPlacement();
    QDialog::hideEvent(event);
}

void MemoDialog::persistOpenState(bool open) {
    if (m_profileId.isEmpty()) {
        return;
    }
    QSettings settings;
    settings.setValue(memoOpenSettingsKey(m_profileId), open);
}

void MemoDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (!m_placementRestored) {
        restorePlacement();
        m_placementRestored = true;
    }
    persistOpenState(true);
}

void MemoDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    const QPalette pal = palette();
    const QColor surface = pal.color(QPalette::Base);
    const QColor border = pal.color(QPalette::Mid);
    const bool dark = isDarkTheme(pal);

    const QRect full = rect();
    const QRect noteRect = full.adjusted(0, 0, -kShadowSize, -kShadowSize);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(dark ? QColor(0, 0, 0, 72) : QColor(0, 0, 0, 38));
    painter.drawRoundedRect(noteRect.translated(kShadowSize, kShadowSize), kNoteRadius, kNoteRadius);

    painter.setBrush(surface);
    painter.setPen(QPen(border, 1));
    painter.drawRoundedRect(noteRect, kNoteRadius, kNoteRadius);

    const int ruleY = noteRect.top() + m_headerHeight;
    painter.setPen(QPen(border, 1));
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
        if (m_dragging) {
            persistPlacement();
        }
        m_dragging = false;
    }
    QDialog::mouseReleaseEvent(event);
}

void MemoDialog::persistPlacement() {
    const QRect geometry = frameGeometry();
    if (!geometry.isValid() || geometry.width() < minimumWidth() || geometry.height() < minimumHeight()) {
        return;
    }

    QScreen* widgetScreen = QWidget::screen();
    if (!widgetScreen && windowHandle()) {
        widgetScreen = windowHandle()->screen();
    }

    QSettings settings;
    settings.setValue(QStringLiteral("memo/placement/screenName"),
                      widgetScreen ? widgetScreen->name() : QString());
    settings.setValue(QStringLiteral("memo/placement/x"), geometry.x());
    settings.setValue(QStringLiteral("memo/placement/y"), geometry.y());
    settings.setValue(QStringLiteral("memo/placement/width"), geometry.width());
    settings.setValue(QStringLiteral("memo/placement/height"), geometry.height());
}

void MemoDialog::restorePlacement() {
    QSettings settings;
    if (settings.contains(QStringLiteral("memo/placement/width"))) {
        const QString screenName = settings.value(QStringLiteral("memo/placement/screenName")).toString();
        const int x = settings.value(QStringLiteral("memo/placement/x")).toInt();
        const int y = settings.value(QStringLiteral("memo/placement/y")).toInt();
        const int width = settings.value(QStringLiteral("memo/placement/width")).toInt();
        const int height = settings.value(QStringLiteral("memo/placement/height")).toInt();
        if (width > 0 && height > 0) {
            QScreen* targetScreen = screenByName(screenName);
            if (!targetScreen) {
                targetScreen = QGuiApplication::primaryScreen();
            }
            if (targetScreen) {
                assignDialogToScreen(this, targetScreen);
                setGeometry(clampWindowRectToScreen(QRect(x, y, width, height), targetScreen));
                return;
            }
        }
    }

    const QByteArray legacyGeometry =
        settings.value(QStringLiteral("memo/geometry")).toByteArray();
    if (!legacyGeometry.isEmpty()) {
        restoreGeometry(legacyGeometry);
        persistPlacement();
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

    const QPalette pal = palette();
    applyLabelTextColor(m_profileLabel, secondaryHintTextColor(pal));
    m_profileLabel->setStyleSheet(QStringLiteral("background: transparent; padding-right: 28px;"));

    if (m_closeButton) {
        m_closeButton->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  color: palette(windowText);"
            "  background: transparent;"
            "  border: none;"
            "  font-size: 16px;"
            "  font-weight: bold;"
            "  border-radius: 12px;"
            "}"
            "QPushButton:hover {"
            "  color: palette(text);"
            "  background: palette(midlight);"
            "}"));
    }
}
