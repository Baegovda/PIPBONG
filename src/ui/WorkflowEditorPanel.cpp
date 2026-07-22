#include "ui/WorkflowEditorPanel.h"

#include "ui/widgets/ListColumnHeaderWidget.h"
#include "ui/widgets/WorkflowRunStatusBar.h"

#include "app/MainWindow.h"
#include "app/PerfTrace.h"
#include "core/workflow/BlockFactory.h"
#include "core/workflow/blocks/ClickBlock.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "core/workflow/WorkflowLoopRegion.h"
#include "core/input/HotkeyBinding.h"
#include "core/input/InputSimulator.h"
#include "model/Feature.h"
#include "ui/BlockListWidget.h"
#include "ui/HotkeyBindingIcon.h"
#include "ui/editors/WorkflowLoopRegionsDialog.h"
#include "ui/UiResizeHandle.h"
#include "ui/UiStrings.h"
#include "ui/editors/BlockEditorDialog.h"
#include "ui/editors/ImageFindPollIntervalPrefs.h"

#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelection>

#include <memory>
#include <QItemSelectionModel>
#include <QAbstractItemView>
#include <QKeySequence>
#include <QLabel>
#include <QRadialGradient>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QTimer>

#include "app/PointerFeedbackSettings.h"
#include "core/capture/ScreenCapture.h"
#include "ui/editors/WorkflowMatchFeedbackOverlay.h"
#include "ui/editors/WorkflowRoiFlashOverlay.h"
#include <QSplitter>
#include <QFrame>
#include <QVBoxLayout>

#include <QtMath>

#include <algorithm>
#include <list>

namespace {

std::vector<CaptureRegion> physicalCustomRegionsForSelectionOverlay(const ImageFindBlock& block) {
    std::vector<CaptureRegion> physical;
    physical.reserve(block.customRegionsWindowPercent.size());
    for (const PercentRegion& percent : block.customRegionsWindowPercent) {
        if (percent.width <= 0.0 || percent.height <= 0.0) {
            continue;
        }
        physical.push_back(ScreenCapture::resolveWindowPercentRegion(percent));
    }
    return physical;
}

void showImageFindSelectionRoiOverlay(const ImageFindBlock& block) {
#ifdef _WIN32
    if (!ScreenCapture::findTargetWindow()) {
        WorkflowImageFindSelectionRoiOverlay::dismissAll();
        return;
    }

    const std::vector<CaptureRegion> physicalRegions = physicalCustomRegionsForSelectionOverlay(block);
    const CaptureRegion physicalLegacy =
        physicalRegions.empty() ? block.customRegion : physicalRegions.front();
    std::vector<CaptureRegion> overlayRegions = physicalRegions;
    if (block.searchArea != SearchArea::CustomRegion) {
        overlayRegions.clear();
    }

    WorkflowImageFindSelectionRoiOverlay::showSearchArea(block.searchArea,
                                                           physicalLegacy,
                                                           block.percentRegion,
                                                           overlayRegions);
#else
    (void)block;
#endif
}

} // namespace

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr int kThumbnailSize = 32;
constexpr int kMaxThumbnailCacheEntries = 128;
constexpr const char* kLastBulkInsertWaitMsKey = "ui/state/workflowEditor/lastBulkInsertWaitMs";
constexpr int kDefaultBulkInsertWaitMs = 500;

QString thumbnailCacheKey(const QString& resolvedPath) {
    const QFileInfo info(resolvedPath);
    if (!info.exists()) {
        return resolvedPath;
    }
    return resolvedPath + QLatin1Char('|') + QString::number(info.lastModified().toMSecsSinceEpoch());
}

QHash<QString, QPixmap> g_thumbnailCache;
std::list<QString> g_thumbnailCacheLru;

QPixmap cachedBlockThumbnail(const QString& resolvedPath, const QPixmap& pixmap) {
    if (pixmap.isNull() || resolvedPath.isEmpty()) {
        return pixmap;
    }
    const QString key = thumbnailCacheKey(resolvedPath);
    if (g_thumbnailCache.contains(key)) {
        g_thumbnailCacheLru.remove(key);
        g_thumbnailCacheLru.push_front(key);
        return g_thumbnailCache.value(key);
    }
    g_thumbnailCache.insert(key, pixmap);
    g_thumbnailCacheLru.push_front(key);
    while (g_thumbnailCacheLru.size() > kMaxThumbnailCacheEntries) {
        const QString evictKey = g_thumbnailCacheLru.back();
        g_thumbnailCacheLru.pop_back();
        g_thumbnailCache.remove(evictKey);
    }
    return pixmap;
}

qreal workflowPreviewDevicePixelRatio() {
    qreal dpr = 2.0;
    if (QGuiApplication::instance()) {
        if (const QScreen* screen = QGuiApplication::primaryScreen()) {
            dpr = qMax(2.0, screen->devicePixelRatio());
        }
    }
    return dpr;
}

QPixmap createWorkflowPreviewCanvas(int logicalSize = kThumbnailSize) {
    const qreal dpr = workflowPreviewDevicePixelRatio();
    QPixmap pixmap(qRound(logicalSize * dpr), qRound(logicalSize * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

void beginWorkflowPreviewPaint(QPainter& painter) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
}

int nearestPrecedingImageFindBlockIndex(const std::vector<std::unique_ptr<Block>>& blocks,
                                        int fromBlockIndex) {
    for (int i = fromBlockIndex - 1; i >= 0; --i) {
        if (blocks[static_cast<size_t>(i)]->type() == BlockType::ImageFind) {
            return i;
        }
    }
    return -1;
}

bool resolveClickBlockPreviewClientPoint(const ClickBlock& click,
                                         int clickBlockIndex,
                                         const Workflow& workflow,
                                         const QVector<bool>& rowMatchMatched,
                                         const QVector<bool>& rowMatchHasClientPoint,
                                         const QVector<int>& rowMatchClientX,
                                         const QVector<int>& rowMatchClientY,
                                         int& clientX,
                                         int& clientY) {
#ifdef _WIN32
    HWND hwnd = ScreenCapture::findTargetWindow();
    if (!hwnd) {
        return false;
    }

    auto toClientPoint = [&](int pointX, int pointY, bool useClientCoordinates) -> bool {
        if (useClientCoordinates) {
            clientX = pointX;
            clientY = pointY;
            return true;
        }
        int screenX = pointX;
        int screenY = pointY;
        return InputSimulator::screenToClient(hwnd, screenX, screenY, clientX, clientY);
    };

    switch (click.target) {
    case ClickBlock::ClickTarget::Fixed:
        return toClientPoint(click.x, click.y, click.useClientCoordinates);
    case ClickBlock::ClickTarget::CurrentPosition: {
        int screenX = 0;
        int screenY = 0;
        if (!InputSimulator::getCursorScreenPosition(screenX, screenY)) {
            return false;
        }
        return InputSimulator::screenToClient(hwnd, screenX, screenY, clientX, clientY);
    }
    case ClickBlock::ClickTarget::LastMatch: {
        int matchX = 0;
        int matchY = 0;
        bool foundMatch = false;
        for (int i = clickBlockIndex - 1; i >= 0; --i) {
            if (i >= rowMatchMatched.size() || !rowMatchMatched[i]) {
                continue;
            }
            if (i >= rowMatchHasClientPoint.size() || !rowMatchHasClientPoint[i]) {
                continue;
            }
            if (i >= rowMatchClientX.size() || i >= rowMatchClientY.size()) {
                continue;
            }
            const auto& blocks = workflow.blocks();
            if (i < 0 || i >= static_cast<int>(blocks.size())
                || blocks[static_cast<size_t>(i)]->type() != BlockType::ImageFind) {
                continue;
            }
            matchX = rowMatchClientX[i];
            matchY = rowMatchClientY[i];
            foundMatch = true;
            break;
        }
        if (!foundMatch) {
            return false;
        }
        if (click.lastMatchRelativeOffset) {
            matchX += click.x;
            matchY += click.y;
        }
        clientX = matchX;
        clientY = matchY;
        return true;
    }
    }
#else
    (void)click;
    (void)clickBlockIndex;
    (void)workflow;
    (void)rowMatchMatched;
    (void)rowMatchHasClientPoint;
    (void)rowMatchClientX;
    (void)rowMatchClientY;
    (void)clientX;
    (void)clientY;
#endif
    return false;
}

int lastBulkInsertWaitMs() {
    QSettings settings;
    return snapWaitDelayMs(settings.value(kLastBulkInsertWaitMsKey, kDefaultBulkInsertWaitMs).toInt());
}

void saveLastBulkInsertWaitMs(int ms) {
    QSettings settings;
    settings.setValue(kLastBulkInsertWaitMsKey, snapWaitDelayMs(ms));
}

bool loopRegionOverlaps(const std::vector<WorkflowLoopRegion>& regions,
                        int startIndex,
                        int endIndex,
                        const std::string& excludeId = {}) {
    for (const WorkflowLoopRegion& region : regions) {
        if (!excludeId.empty() && region.id == excludeId) {
            continue;
        }
        if (startIndex <= region.endIndex && region.startIndex <= endIndex) {
            return true;
        }
    }
    return false;
}

QRectF mouseBodyRect() {
    return QRectF(7.0, 4.0, 18.0, 24.0);
}

void drawMouseBody(QPainter& painter, const QRectF& body) {
    painter.setPen(QPen(QColor(90, 90, 90), 1.2));
    painter.setBrush(QColor(245, 245, 245));
    painter.drawRoundedRect(body, 4.0, 4.0);

    painter.setPen(QPen(QColor(90, 90, 90), 1.0));
    painter.drawLine(body.center().x(), body.top() + 1.0, body.center().x(), body.top() + body.height() * 0.55);
    painter.drawLine(body.left() + 2.0,
                     body.top() + body.height() * 0.55,
                     body.right() - 2.0,
                     body.top() + body.height() * 0.55);
}

QPixmap clickBlockPreviewIcon(MouseButton button) {
    QPixmap pixmap = createWorkflowPreviewCanvas();

    QPainter painter(&pixmap);
    beginWorkflowPreviewPaint(painter);

    const QRectF body = mouseBodyRect();
    const QColor accent(74, 144, 217);

    switch (button) {
    case MouseButton::Left: {
        drawMouseBody(painter, body);
        const QRectF leftButton(body.left(), body.top(), body.width() * 0.5, body.height() * 0.55);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(leftButton, 3.0, 3.0);
        break;
    }
    case MouseButton::Right: {
        drawMouseBody(painter, body);
        const QRectF rightButton(body.left() + body.width() * 0.5, body.top(), body.width() * 0.5, body.height() * 0.55);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(rightButton, 3.0, 3.0);
        break;
    }
    case MouseButton::Middle: {
        drawMouseBody(painter, body);
        const QRectF wheelSlot(body.center().x() - 2.5, body.top() + body.height() * 0.58, 5.0, 7.0);
        painter.setPen(QPen(QColor(90, 90, 90), 1.0));
        painter.setBrush(accent);
        painter.drawRoundedRect(wheelSlot, 2.0, 2.0);
        break;
    }
    case MouseButton::Back:
        drawMouseSideNavigationIcon(painter, false);
        break;
    case MouseButton::Forward:
        drawMouseSideNavigationIcon(painter, true);
        break;
    case MouseButton::WheelUp:
    case MouseButton::WheelDown: {
        const QRectF wheelBody(10.0, 6.0, 12.0, 20.0);
        painter.setPen(QPen(QColor(90, 90, 90), 1.2));
        painter.setBrush(QColor(245, 245, 245));
        painter.drawRoundedRect(wheelBody, 3.0, 3.0);

        painter.setPen(QPen(accent, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const qreal cx = wheelBody.center().x();
        const qreal cy = wheelBody.center().y();
        if (button == MouseButton::WheelUp) {
            painter.drawLine(QPointF(cx, cy + 4.0), QPointF(cx, cy - 5.0));
            painter.drawLine(QPointF(cx, cy - 5.0), QPointF(cx - 4.0, cy - 1.0));
            painter.drawLine(QPointF(cx, cy - 5.0), QPointF(cx + 4.0, cy - 1.0));
        } else {
            painter.drawLine(QPointF(cx, cy - 4.0), QPointF(cx, cy + 5.0));
            painter.drawLine(QPointF(cx, cy + 5.0), QPointF(cx - 4.0, cy + 1.0));
            painter.drawLine(QPointF(cx, cy + 5.0), QPointF(cx + 4.0, cy + 1.0));
        }
        break;
    }
    }

    return pixmap;
}

QString compactKeyLabel(int virtualKey) {
#ifdef _WIN32
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return QString(QChar(virtualKey));
    }
    if (virtualKey >= '0' && virtualKey <= '9') {
        return QString(QChar(virtualKey));
    }
    switch (virtualKey) {
    case VK_SPACE:
        return QStringLiteral("Sp");
    case VK_RETURN:
        return QStringLiteral("Enter");
    case VK_ESCAPE:
        return QStringLiteral("Esc");
    case VK_TAB:
        return QStringLiteral("Tab");
    case VK_BACK:
        return QStringLiteral("⌫");
    case VK_DELETE:
        return QStringLiteral("Del");
    case VK_HOME:
        return QStringLiteral("Hm");
    case VK_END:
        return QStringLiteral("End");
    case VK_PRIOR:
        return QStringLiteral("PU");
    case VK_NEXT:
        return QStringLiteral("PD");
    case VK_LEFT:
        return QStringLiteral("←");
    case VK_RIGHT:
        return QStringLiteral("→");
    case VK_UP:
        return QStringLiteral("↑");
    case VK_DOWN:
        return QStringLiteral("↓");
    case VK_F1:
    case VK_F2:
    case VK_F3:
    case VK_F4:
    case VK_F5:
    case VK_F6:
    case VK_F7:
    case VK_F8:
    case VK_F9:
    case VK_F10:
    case VK_F11:
    case VK_F12:
        return QStringLiteral("F%1").arg(virtualKey - VK_F1 + 1);
    default: {
        const int qtKey = HotkeyBinding::virtualKeyToQtKey(virtualKey);
        QString native = QKeySequence(qtKey).toString(QKeySequence::NativeText);
        native.replace(QStringLiteral("Return"), QStringLiteral("Enter"));
        if (!native.isEmpty() && native.size() <= 6) {
            return native;
        }
        return QStringLiteral("K");
    }
    }
#else
    (void)virtualKey;
    return QStringLiteral("K");
#endif
}

QString modifierPrefix(const KeyPressModifierActions& modifierActions) {
    QStringList parts;
    if (modifierActions.ctrl != ModifierKeyAction::None) {
        parts << QStringLiteral("C");
    }
    if (modifierActions.alt != ModifierKeyAction::None) {
        parts << QStringLiteral("A");
    }
    if (modifierActions.shift != ModifierKeyAction::None) {
        parts << QStringLiteral("S");
    }
    return parts.join(QStringLiteral("+"));
}

QPixmap keyPressPreviewIcon(int virtualKey,
                            const KeyPressModifierActions& modifierActions,
                            bool useMainKey) {
    QPixmap pixmap = createWorkflowPreviewCanvas();

    QPainter painter(&pixmap);
    beginWorkflowPreviewPaint(painter);

    const QColor accent(59, 130, 246);
    const QColor ink(24, 32, 44);
    const QColor border(148, 163, 184);
    const QRectF cap(4.0, 5.0, 24.0, 22.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(15, 23, 42, 28));
    painter.drawRoundedRect(cap.translated(0.0, 1.2), 6.0, 6.0);

    QLinearGradient face(cap.topLeft(), cap.bottomLeft());
    face.setColorAt(0.0, QColor(255, 255, 255));
    face.setColorAt(0.45, QColor(248, 250, 252));
    face.setColorAt(1.0, QColor(226, 232, 240));
    painter.setBrush(face);
    painter.setPen(QPen(border, 1.0));
    painter.drawRoundedRect(cap, 6.0, 6.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 200));
    painter.drawRoundedRect(QRectF(cap.left() + 1.5, cap.top() + 1.0, cap.width() - 3.0, 5.0), 4.0, 4.0);

    const QString modText = modifierPrefix(modifierActions);
    const QString keyText = useMainKey ? compactKeyLabel(virtualKey) : QString();
    const bool hasBoth = !modText.isEmpty() && !keyText.isEmpty();

    if (!modText.isEmpty()) {
        QFont modFont(QStringLiteral("Segoe UI"));
        modFont.setPixelSize(hasBoth ? 8 : 10);
        modFont.setWeight(QFont::DemiBold);
        painter.setFont(modFont);
        painter.setPen(accent);
        const QRectF modRect = hasBoth ? QRectF(cap.left(), cap.top() + 2.0, cap.width(), 8.0)
                                     : cap.adjusted(1.0, 0.0, -1.0, 0.0);
        painter.drawText(modRect, Qt::AlignHCenter | Qt::AlignVCenter, modText);
    }

    if (!keyText.isEmpty()) {
        QFont keyFont(QStringLiteral("Segoe UI"));
        const int len = keyText.size();
        keyFont.setPixelSize(len >= 5 ? 9 : (len >= 3 ? 10 : 13));
        keyFont.setWeight(QFont::Bold);
        painter.setFont(keyFont);
        painter.setPen(ink);
        const QRectF keyRect = hasBoth
                                   ? QRectF(cap.left(), cap.top() + 9.0, cap.width(), cap.height() - 10.0)
                                   : cap.adjusted(1.0, 0.5, -1.0, -0.5);
        painter.drawText(keyRect, Qt::AlignCenter, keyText);
    }

    return pixmap;
}

QPixmap waitBlockPreviewIcon() {
    QPixmap pixmap = createWorkflowPreviewCanvas();

    QPainter painter(&pixmap);
    beginWorkflowPreviewPaint(painter);

    const QColor accent(59, 130, 246);
    const QColor ring(148, 163, 184);
    const QRectF dial(5.0, 4.0, 22.0, 22.0);
    const QPointF center = dial.center();

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(15, 23, 42, 24));
    painter.drawEllipse(dial.translated(0.0, 1.0));

    QRadialGradient face(center, dial.width() * 0.5);
    face.setColorAt(0.0, QColor(255, 255, 255));
    face.setColorAt(0.72, QColor(241, 245, 249));
    face.setColorAt(1.0, QColor(226, 232, 240));
    painter.setBrush(face);
    painter.setPen(QPen(ring, 1.0));
    painter.drawEllipse(dial);

    const QRectF arcRect = dial.adjusted(3.0, 3.0, -3.0, -3.0);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(203, 213, 225), 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcRect, 0, 360 * 16);
    painter.setPen(QPen(accent, 2.2, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcRect, 90 * 16, -300 * 16);

    painter.setPen(QPen(accent, 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, QPointF(center.x(), center.y() - dial.height() * 0.17));
    painter.setPen(QPen(accent, 1.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, QPointF(center.x() + dial.width() * 0.19, center.y() + dial.height() * 0.05));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255));
    painter.drawEllipse(center, 2.2, 2.2);
    painter.setBrush(accent);
    painter.drawEllipse(center, 1.4, 1.4);

    return pixmap;
}

QPixmap mouseMovePreviewIcon() {
    QPixmap pixmap = createWorkflowPreviewCanvas();

    QPainter painter(&pixmap);
    beginWorkflowPreviewPaint(painter);

    const QRectF body = mouseBodyRect();
    const QColor accent(74, 144, 217);
    drawMouseBody(painter, body);

    painter.setPen(QPen(accent, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF from(body.center().x() - 5.0, body.center().y() + 2.0);
    const QPointF to(body.center().x() + 7.0, body.center().y() - 6.0);
    painter.drawLine(from, to);
    painter.drawLine(to, QPointF(to.x() - 4.0, to.y()));
    painter.drawLine(to, QPointF(to.x(), to.y() + 4.0));

    return pixmap;
}

QPixmap textBlockPreviewIcon() {
    QPixmap pixmap = createWorkflowPreviewCanvas();

    QPainter painter(&pixmap);
    beginWorkflowPreviewPaint(painter);

    const QRectF doc(7.0, 5.0, 18.0, 22.0);
    const QColor accent(74, 144, 217);
    painter.setPen(QPen(QColor(120, 120, 120), 1.0));
    painter.setBrush(QColor(248, 248, 248));
    painter.drawRoundedRect(doc, 2.0, 2.0);

    painter.setPen(QPen(accent, 1.6, Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < 4; ++i) {
        const qreal y = 10.0 + i * 4.0;
        const qreal width = (i == 3) ? 8.0 : 12.0;
        painter.drawLine(QPointF(10.0, y), QPointF(10.0 + width, y));
    }

    return pixmap;
}

QPixmap loadBlockThumbnail(const Block& block, const QString& projectDirectory) {
    if (block.type() == BlockType::Click) {
        const auto* clickBlock = dynamic_cast<const ClickBlock*>(&block);
        if (clickBlock) {
            if (clickBlock->action == ClickAction::MoveOnly) {
                return mouseMovePreviewIcon();
            }
            return clickBlockPreviewIcon(clickBlock->button);
        }
        return {};
    }

    if (block.type() == BlockType::KeyPress) {
        const auto* keyBlock = dynamic_cast<const KeyPressBlock*>(&block);
        if (keyBlock) {
            return keyPressPreviewIcon(keyBlock->virtualKey, keyBlock->modifierActions, keyBlock->useMainKey);
        }
        return {};
    }

    if (block.type() == BlockType::Wait) {
        return waitBlockPreviewIcon();
    }

    if (block.type() == BlockType::Text) {
        return textBlockPreviewIcon();
    }

    if (block.type() != BlockType::ImageFind) {
        return {};
    }

    const auto* imageFind = dynamic_cast<const ImageFindBlock*>(&block);
    const std::string primaryTemplate = imageFind ? imageFind->primaryTemplatePath() : std::string{};
    if (!imageFind || primaryTemplate.empty()) {
        return {};
    }

    const QString relativePath = QString::fromStdString(primaryTemplate);
    const QFileInfo fileInfo(relativePath);
    const QString resolvedPath = fileInfo.isAbsolute()
                                     ? relativePath
                                     : QDir(projectDirectory).filePath(relativePath);

    const QString cacheKey = thumbnailCacheKey(resolvedPath);
    if (g_thumbnailCache.contains(cacheKey)) {
        g_thumbnailCacheLru.remove(cacheKey);
        g_thumbnailCacheLru.push_front(cacheKey);
        return g_thumbnailCache.value(cacheKey);
    }

    QPixmap pixmap(resolvedPath);
    if (pixmap.isNull()) {
        return {};
    }
    const QPixmap scaled = pixmap.scaled(kThumbnailSize,
                                       kThumbnailSize,
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
    return cachedBlockThumbnail(resolvedPath, scaled);
}

} // namespace

namespace {

constexpr int kMinWorkflowBlockListPanePx = 120;
constexpr int kMinWorkflowToolsPanePx = 88;
constexpr int kDefaultWorkflowBlockListPanePx = 480;
constexpr int kDefaultWorkflowToolsPanePx = 120;

void applyWorkflowSplitterDefaults(QSplitter* splitter) {
    if (!splitter) {
        return;
    }
    QSignalBlocker blocker(splitter);
    splitter->setSizes({kDefaultWorkflowBlockListPanePx, kDefaultWorkflowToolsPanePx});
}

} // namespace

void WorkflowEditorPanel::clampWorkflowSplitterSizes() {
    if (!m_workflowSplitter || m_workflowSplitter->count() < 2) {
        return;
    }

    QWidget* listPane = m_workflowSplitter->widget(0);
    QWidget* toolsPane = m_workflowSplitter->widget(1);
    if (!listPane || !toolsPane) {
        return;
    }

    const int minList = qMax(kMinWorkflowBlockListPanePx, listPane->minimumHeight());
    const int minTools = qMax(kMinWorkflowToolsPanePx, toolsPane->minimumHeight());
    const int handle = m_workflowSplitter->handleWidth();
    const int total = m_workflowSplitter->height();
    if (total <= handle) {
        return;
    }

    const int available = total - handle;
    if (available < minList + minTools) {
        return;
    }

    QList<int> sizes = m_workflowSplitter->sizes();
    int top = sizes.value(0, 0);
    int bottom = sizes.value(1, 0);

    if (top + bottom <= 0) {
        applyWorkflowSplitterDefaults(m_workflowSplitter);
        return;
    }

    top = qBound(minList, top, available - minTools);
    bottom = available - top;
    if (bottom < minTools) {
        bottom = minTools;
        top = qBound(minList, available - bottom, available - minTools);
    }

    if (top != sizes.value(0) || bottom != sizes.value(1)) {
        QSignalBlocker blocker(m_workflowSplitter);
        m_workflowSplitter->setSizes({top, bottom});
    }
}

WorkflowEditorPanel::WorkflowEditorPanel(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void WorkflowEditorPanel::setupUi() {
    auto* layout = new QVBoxLayout(this);

    m_runStatusBar = new WorkflowRunStatusBar(this);
    connect(m_runStatusBar, &WorkflowRunStatusBar::runToggleRequested, this, [this]() {
        if (!m_feature) {
            return;
        }
        emit featureRunRequested(QString::fromStdString(m_feature->id()));
    });

    m_blockList = new BlockListWidget(this);

    auto* blockListFrame = new QFrame(this);
    blockListFrame->setObjectName(QStringLiteral("blockListTableFrame"));
    blockListFrame->setFrameShape(QFrame::NoFrame);
    auto* blockListLayout = new QVBoxLayout(blockListFrame);
    blockListLayout->setContentsMargins(0, 0, 0, 0);
    blockListLayout->setSpacing(0);

    m_blockListHeader = new ListColumnHeaderWidget(blockListFrame);
    BlockListWidget::wireListColumnHeader(m_blockListHeader, m_blockList);
    blockListLayout->addWidget(m_blockListHeader);
    blockListLayout->addWidget(m_blockList, 1);

    connect(m_blockList, &QTableWidget::cellClicked, this,
            [this](int row, int column) {
                if (column != BlockListWidget::PreviewColumn || m_loopRegionPickActive) {
                    return;
                }
                const int blockRow = m_blockList->blockRowForTableRow(row);
                if (blockRow >= 0) {
                    onBlockDoubleClicked(blockRow);
                }
            });
    connect(m_blockList, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int column) {
                if (column == BlockListWidget::PreviewColumn) {
                    return;
                }
                const int blockRow = m_blockList->blockRowForTableRow(row);
                if (blockRow >= 0) {
                    onBlockDoubleClicked(blockRow);
                    return;
                }
                const QString regionId = m_blockList->loopRegionIdForTableRow(row);
                if (!regionId.isEmpty()) {
                    editLoopRegion(regionId);
                }
            });
    connect(m_blockList, &BlockListWidget::blockRowsReordered, this,
            &WorkflowEditorPanel::onBlockRowsReordered);
    connect(m_blockList, &BlockListWidget::copyRequested, this, &WorkflowEditorPanel::onCopyBlocks);
    connect(m_blockList, &BlockListWidget::pasteRequested, this, &WorkflowEditorPanel::onPasteBlocks);
    connect(m_blockList, &BlockListWidget::deleteRequested, this, &WorkflowEditorPanel::onDeleteBlocks);
    connect(m_blockList, &BlockListWidget::editRequested, this, &WorkflowEditorPanel::onEditBlock);
    connect(m_blockList, &BlockListWidget::undoRequested, this, &WorkflowEditorPanel::onUndo);
    connect(m_blockList, &BlockListWidget::redoRequested, this, &WorkflowEditorPanel::onRedo);

    m_clickSelectionPreviewTimer = new QTimer(this);
    m_clickSelectionPreviewTimer->setTimerType(Qt::CoarseTimer);
    connect(m_clickSelectionPreviewTimer, &QTimer::timeout, this,
            &WorkflowEditorPanel::onClickBlockSelectionPreviewTick);

    m_imageFindSelectionRoiTimer = new QTimer(this);
    m_imageFindSelectionRoiTimer->setTimerType(Qt::CoarseTimer);
    connect(m_imageFindSelectionRoiTimer, &QTimer::timeout, this,
            &WorkflowEditorPanel::onImageFindSelectionRoiPreviewTick);
    if (m_blockList->selectionModel()) {
        connect(m_blockList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                &WorkflowEditorPanel::onBlockSelectionChanged);
    }

    auto* toolsPane = new QWidget(this);
    auto* toolsLayout = new QVBoxLayout(toolsPane);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(6);

    auto* addGroup = new QGroupBox(tr("블록 추가"), toolsPane);
    auto* addRow = new QHBoxLayout(addGroup);
    addRow->setSpacing(6);

    const BlockType addTypes[] = {BlockType::ImageFind,
                                  BlockType::Click,
                                  BlockType::KeyPress,
                                  BlockType::Wait,
                                  BlockType::Text};
    for (const BlockType type : addTypes) {
        auto* button = new QPushButton(blockTypeDisplayName(type), addGroup);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setToolTip(tr("%1 블록을 워크플로우에 추가").arg(blockTypeDisplayName(type)));
        connect(button, &QPushButton::clicked, this, [this, type]() { addBlockOfType(type); });
        m_addTypeButtons.append(button);
        addRow->addWidget(button, 2);
    }

    m_removeAllWaitButton = new QPushButton(tr("전체 딜레이 삭제"), addGroup);
    m_removeAllWaitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_removeAllWaitButton->setToolTip(tr("워크플로우에 있는 딜레이 블록을 모두 제거합니다. Ctrl+Z로 되돌릴 수 있습니다."));
    addRow->addWidget(m_removeAllWaitButton, 3);

    m_insertWaitBetweenButton = new QPushButton(tr("전체 딜레이 추가"), addGroup);
    m_insertWaitBetweenButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_insertWaitBetweenButton->setToolTip(
        tr("인접한 블록 사이마다 지정한 시간(ms)의 딜레이 블록을 삽입합니다. Ctrl+Z로 되돌릴 수 있습니다."));
    addRow->addWidget(m_insertWaitBetweenButton, 3);

    m_loopRegionsButton = new QPushButton(tr("구간 반복"), addGroup);
    m_loopRegionsButton->setCheckable(true);
    m_loopRegionsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_loopRegionsButton->setToolTip(
        tr("클릭 후 워크플로우 목록에서 블록을 드래그하여 반복 구간을 선택합니다. "
           "우클릭하면 기존 구간 목록을 엽니다."));
    m_loopRegionsButton->setContextMenuPolicy(Qt::CustomContextMenu);
    addRow->addWidget(m_loopRegionsButton, 3);

    toolsLayout->addWidget(addGroup);

    blockListFrame->setMinimumHeight(kMinWorkflowBlockListPanePx);
    toolsPane->setMinimumHeight(kMinWorkflowToolsPanePx);

    auto* workflowPane = new QWidget(this);
    auto* workflowLayout = new QVBoxLayout(workflowPane);
    workflowLayout->setContentsMargins(0, 0, 0, 0);

    m_workflowSplitter = new QSplitter(Qt::Vertical, workflowPane);
    UiResizeHandle::configureSplitter(m_workflowSplitter);
    m_workflowSplitter->addWidget(blockListFrame);
    m_workflowSplitter->addWidget(toolsPane);
    m_workflowSplitter->setStretchFactor(0, 1);
    m_workflowSplitter->setStretchFactor(1, 0);
    applyWorkflowSplitterDefaults(m_workflowSplitter);

    connect(m_workflowSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        clampWorkflowSplitterSizes();
    });

    workflowLayout->addWidget(m_workflowSplitter, 1);

    layout->addWidget(m_runStatusBar);
    layout->addWidget(workflowPane, 1);

    connect(m_removeAllWaitButton, &QPushButton::clicked, this, &WorkflowEditorPanel::onRemoveAllWaitBlocks);
    connect(m_insertWaitBetweenButton, &QPushButton::clicked, this, &WorkflowEditorPanel::onInsertWaitBetweenBlocks);
    connect(m_loopRegionsButton, &QPushButton::clicked, this, &WorkflowEditorPanel::onManageLoopRegions);
    connect(m_loopRegionsButton,
            &QPushButton::customContextMenuRequested,
            this,
            &WorkflowEditorPanel::onLoopRegionsButtonContextMenu);
    connect(m_blockList,
            &BlockListWidget::loopRegionRangePicked,
            this,
            &WorkflowEditorPanel::onLoopRegionRangePicked);
    connect(m_blockList,
            &BlockListWidget::loopRegionPickCancelled,
            this,
            &WorkflowEditorPanel::onLoopRegionPickCancelled);
    connect(m_blockList,
            &BlockListWidget::loopRegionEditRequested,
            this,
            &WorkflowEditorPanel::onLoopRegionEditRequested);
    connect(m_blockList,
            &BlockListWidget::loopRegionDeleteRequested,
            this,
            &WorkflowEditorPanel::onLoopRegionDeleteRequested);
    connect(m_blockList,
            &BlockListWidget::imageFindThresholdChanged,
            this,
            &WorkflowEditorPanel::onImageFindThresholdChanged);
    connect(m_blockList,
            &BlockListWidget::imageFindRoiCorrectionChanged,
            this,
            &WorkflowEditorPanel::onImageFindRoiCorrectionChanged);
}

QSplitter* WorkflowEditorPanel::workflowSplitter() const {
    return m_workflowSplitter;
}

void WorkflowEditorPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    clampWorkflowSplitterSizes();
}

void WorkflowEditorPanel::clearBlockMatchResults() {
    if (m_feature) {
        m_featureRunFeedback.erase(m_feature->id());
    }
    clearCurrentRunFeedbackVectors();
    m_blockList->clearBlockMatchResults();
}

void WorkflowEditorPanel::clearCurrentRunFeedbackVectors() {
    m_rowMatchImages.clear();
    m_rowMatchConfidences.clear();
    m_rowMatchThresholds.clear();
    m_rowMatchMatched.clear();
    m_rowMatchHasClientPoint.clear();
    m_rowMatchClientX.clear();
    m_rowMatchClientY.clear();
    m_rowBlockDurations.clear();
    m_rowImageFindMatchDurations.clear();
    m_rowImageFindAttemptCounts.clear();
    m_rowImageFindReturnCounts.clear();
    m_rowImageFindRetryCounts.clear();
}

void WorkflowEditorPanel::persistRunFeedbackForCurrentFeature() {
    if (m_feature) {
        saveRunFeedbackForFeature(m_feature->id());
    }
}

void WorkflowEditorPanel::saveRunFeedbackForFeature(const std::string& featureId) {
    if (featureId.empty()) {
        return;
    }

    FeatureRunFeedback feedback;
    feedback.rowMatchImages = m_rowMatchImages;
    feedback.rowMatchConfidences = m_rowMatchConfidences;
    feedback.rowMatchThresholds = m_rowMatchThresholds;
    feedback.rowMatchMatched = m_rowMatchMatched;
    feedback.rowMatchHasClientPoint = m_rowMatchHasClientPoint;
    feedback.rowMatchClientX = m_rowMatchClientX;
    feedback.rowMatchClientY = m_rowMatchClientY;
    feedback.rowBlockDurations = m_rowBlockDurations;
    feedback.rowImageFindMatchDurations = m_rowImageFindMatchDurations;
    feedback.rowImageFindAttemptCounts = m_rowImageFindAttemptCounts;
    feedback.rowImageFindReturnCounts = m_rowImageFindReturnCounts;
    feedback.rowImageFindRetryCounts = m_rowImageFindRetryCounts;
    feedback.activeBlockIndex = m_activeBlockIndex;
    feedback.executionHighlight = m_executionHighlight;
    feedback.hasLoopTiming = m_hasLoopTiming;
    feedback.loopNumber = m_loopNumber;
    feedback.loopElapsedMs = m_loopElapsedMs;
    feedback.loopAverageMs = m_loopAverageMs;
    feedback.loopSuccess = m_loopSuccess;

    const int blockCount = m_blockList->blockCount();
    feedback.rowMatchLockedSuccess.resize(blockCount);
    for (int i = 0; i < blockCount; ++i) {
        feedback.rowMatchLockedSuccess[i] = m_blockList->isMatchSuccessLocked(i);
    }

    m_featureRunFeedback[featureId] = std::move(feedback);
}

void WorkflowEditorPanel::restoreRunFeedbackForFeature(const std::string& featureId) {
    const auto it = m_featureRunFeedback.find(featureId);
    if (it == m_featureRunFeedback.end()) {
        clearCurrentRunFeedbackVectors();
        m_activeBlockIndex = -1;
        m_executionHighlight = BlockListWidget::ExecutionHighlight::None;
        clearLoopTiming();
        return;
    }

    const FeatureRunFeedback& feedback = it->second;
    m_rowMatchImages = feedback.rowMatchImages;
    m_rowMatchConfidences = feedback.rowMatchConfidences;
    m_rowMatchThresholds = feedback.rowMatchThresholds;
    m_rowMatchMatched = feedback.rowMatchMatched;
    m_rowMatchHasClientPoint = feedback.rowMatchHasClientPoint;
    m_rowMatchClientX = feedback.rowMatchClientX;
    m_rowMatchClientY = feedback.rowMatchClientY;
    m_rowBlockDurations = feedback.rowBlockDurations;
    m_rowImageFindMatchDurations = feedback.rowImageFindMatchDurations;
    m_rowImageFindAttemptCounts = feedback.rowImageFindAttemptCounts;
    m_rowImageFindReturnCounts = feedback.rowImageFindReturnCounts;
    m_rowImageFindRetryCounts = feedback.rowImageFindRetryCounts;
    m_activeBlockIndex = feedback.activeBlockIndex;
    m_executionHighlight = feedback.executionHighlight;
    if (feedback.hasLoopTiming) {
        setLoopTiming(feedback.loopNumber, feedback.loopElapsedMs, feedback.loopAverageMs, feedback.loopSuccess);
    } else {
        clearLoopTiming();
    }
}

void WorkflowEditorPanel::clearExecutionHighlight() {
    m_activeBlockIndex = -1;
    m_executionHighlight = BlockListWidget::ExecutionHighlight::None;
    m_blockList->clearActiveRow();
    updateClickBlockSelectionPreview();
    updateImageFindSelectionRoiPreview();
}

void WorkflowEditorPanel::setActiveBlockIndex(int index, BlockListWidget::ExecutionHighlight highlight) {
    m_activeBlockIndex = index;
    m_executionHighlight = index >= 0 ? highlight : BlockListWidget::ExecutionHighlight::None;
    if (m_executionHighlight != BlockListWidget::ExecutionHighlight::None) {
        stopClickBlockSelectionPreview();
        stopImageFindSelectionRoiPreview();
    }
    m_blockList->setActiveRow(index, m_executionHighlight);
}

void WorkflowEditorPanel::notifyImageFindRetry(int blockIndex) {
    if (m_blockList->isMatchSuccessLocked(blockIndex)) {
        return;
    }
    m_activeBlockIndex = blockIndex;
    m_executionHighlight = BlockListWidget::ExecutionHighlight::ImageFindMiss;
    m_blockList->notifyImageFindRetry(blockIndex);
}

void WorkflowEditorPanel::notifyImageFindReturnToPrevious(int sourceBlockIndex, int targetBlockIndex) {
    if (sourceBlockIndex < 0 || targetBlockIndex < 0) {
        return;
    }
    m_blockList->notifyImageFindReturnToPrevious(sourceBlockIndex, targetBlockIndex);
}

bool WorkflowEditorPanel::isBlockMatchSuccessCommitted(int blockIndex) const {
    return m_blockList->isMatchSuccessLocked(blockIndex);
}

void WorkflowEditorPanel::setBlockMatchResult(int blockIndex,
                                              double matchThreshold,
                                              double confidence,
                                              const QPixmap& image,
                                              bool matched,
                                              bool hasClientPoint,
                                              int clientX,
                                              int clientY) {
    if (!matched && m_blockList->isMatchSuccessLocked(blockIndex)) {
        return;
    }

    if (matched) {
        m_activeBlockIndex = blockIndex;
        m_executionHighlight = BlockListWidget::ExecutionHighlight::Success;
    }

    if (blockIndex >= m_rowMatchConfidences.size()) {
        m_rowMatchConfidences.resize(blockIndex + 1, -1.0);
        m_rowMatchThresholds.resize(blockIndex + 1, 0.0);
        m_rowMatchMatched.resize(blockIndex + 1, false);
        m_rowMatchImages.resize(blockIndex + 1);
        m_rowMatchHasClientPoint.resize(blockIndex + 1, false);
        m_rowMatchClientX.resize(blockIndex + 1, 0);
        m_rowMatchClientY.resize(blockIndex + 1, 0);
    }
    m_rowMatchThresholds[blockIndex] = matchThreshold;
    m_rowMatchConfidences[blockIndex] = confidence;
    m_rowMatchMatched[blockIndex] = matched;
    m_rowMatchImages[blockIndex] = image;
    if (matched && hasClientPoint) {
        m_rowMatchHasClientPoint[blockIndex] = true;
        m_rowMatchClientX[blockIndex] = clientX;
        m_rowMatchClientY[blockIndex] = clientY;
    } else if (!matched) {
        m_rowMatchHasClientPoint[blockIndex] = false;
    }
    m_blockList->setBlockMatchResult(blockIndex, matchThreshold, confidence, image, matched);
}

void WorkflowEditorPanel::markBlockMatchSuccess(int blockIndex) {
    if (blockIndex < 0 || !m_feature) {
        return;
    }

    const auto& blocks = m_feature->workflow().blocks();
    if (blockIndex >= static_cast<int>(blocks.size())
        || blocks[blockIndex]->type() != BlockType::ImageFind) {
        return;
    }

    double threshold = static_cast<const ImageFindBlock&>(*blocks[blockIndex]).threshold;
    if (blockIndex < static_cast<int>(m_rowMatchThresholds.size()) && m_rowMatchThresholds[blockIndex] > 0.0) {
        threshold = m_rowMatchThresholds[blockIndex];
    }

    const double confidence =
        blockIndex < static_cast<int>(m_rowMatchConfidences.size()) ? m_rowMatchConfidences[blockIndex] : 0.0;
    const QPixmap image =
        blockIndex < static_cast<int>(m_rowMatchImages.size()) ? m_rowMatchImages[blockIndex] : QPixmap();

    if (confidence > 0.0 || !image.isNull()) {
        setBlockMatchResult(blockIndex, threshold, confidence > 0.0 ? confidence : 1.0, image, true);
    } else if (blockIndex < static_cast<int>(m_rowMatchConfidences.size())
               && m_rowMatchConfidences[blockIndex] >= 0.0) {
        setBlockMatchResult(blockIndex,
                            threshold,
                            m_rowMatchConfidences[blockIndex],
                            image,
                            true);
    }
    m_blockList->commitRowSuccess(blockIndex);
}

void WorkflowEditorPanel::setBlockDuration(int blockIndex, qint64 durationMs) {
    if (blockIndex >= m_rowBlockDurations.size()) {
        m_rowBlockDurations.resize(blockIndex + 1, -1);
    }
    m_rowBlockDurations[blockIndex] = durationMs;
    m_blockList->setBlockDuration(blockIndex, durationMs);
}

void WorkflowEditorPanel::setBlockImageFindMatchDuration(int blockIndex, qint64 matchDurationMs) {
    if (blockIndex >= m_rowImageFindMatchDurations.size()) {
        m_rowImageFindMatchDurations.resize(blockIndex + 1, -1);
    }
    m_rowImageFindMatchDurations[blockIndex] = matchDurationMs;
    m_blockList->setBlockImageFindMatchDuration(blockIndex, matchDurationMs);
}

void WorkflowEditorPanel::setBlockImageFindAttemptCount(int blockIndex, int attemptCount) {
    if (blockIndex < 0) {
        return;
    }
    if (blockIndex >= m_rowImageFindAttemptCounts.size()) {
        m_rowImageFindAttemptCounts.resize(blockIndex + 1, -1);
    }
    m_rowImageFindAttemptCounts[blockIndex] = attemptCount;
    m_blockList->setBlockImageFindAttemptCount(blockIndex, attemptCount);
}

void WorkflowEditorPanel::setBlockImageFindFailureHandlingCounts(int blockIndex,
                                                               int returnToPreviousCount,
                                                               int retryAfterNextCount) {
    if (blockIndex < 0) {
        return;
    }
    if (blockIndex >= m_rowImageFindReturnCounts.size()) {
        m_rowImageFindReturnCounts.resize(blockIndex + 1, -1);
    }
    if (blockIndex >= m_rowImageFindRetryCounts.size()) {
        m_rowImageFindRetryCounts.resize(blockIndex + 1, -1);
    }
    m_rowImageFindReturnCounts[blockIndex] = returnToPreviousCount;
    m_rowImageFindRetryCounts[blockIndex] = retryAfterNextCount;
    m_blockList->setBlockImageFindFailureHandlingCounts(
        blockIndex, returnToPreviousCount, retryAfterNextCount);
}

void WorkflowEditorPanel::setFeature(Feature* feature) {
    if (m_feature && m_feature != feature) {
        saveRunFeedbackForFeature(m_feature->id());
        setLoopRegionPickMode(false);
        stopClickBlockSelectionPreview();
        stopImageFindSelectionRoiPreview();
    }

    m_feature = feature;
    clearWorkflowHistory();

    if (m_blockList) {
        m_blockList->setFeatureRunMode(feature ? feature->runMode() : FeatureRunMode::RepeatCount);
    }

    if (m_feature) {
        restoreRunFeedbackForFeature(m_feature->id());
    } else {
        clearBlockMatchResults();
        clearExecutionHighlight();
        setLoopRegionPickMode(false);
    }

    refresh();

    if (m_feature) {
        const auto it = m_featureRunFeedback.find(m_feature->id());
        if (it != m_featureRunFeedback.end()) {
            for (int i = 0; i < it->second.rowMatchLockedSuccess.size(); ++i) {
                if (it->second.rowMatchLockedSuccess[i]) {
                    markBlockMatchSuccess(i);
                }
            }
        }
    }
}

void WorkflowEditorPanel::setProfileDisplayName(const QString& name) {
    m_profileDisplayName = name.trimmed();
    updateTitleText();
}

void WorkflowEditorPanel::updateTitleText() {
    if (!m_runStatusBar) {
        return;
    }
    m_runStatusBar->setProfileName(m_profileDisplayName);
    if (!m_feature) {
        m_runStatusBar->setFeatureName(QString());
        m_runStatusBar->clearRunMode();
        m_runStatusBar->clearLoopTiming();
        m_runStatusBar->setRunButtonState(false, false);
        return;
    }

    m_runStatusBar->setFeatureName(QString::fromStdString(m_feature->name()));
    m_runStatusBar->setRunMode(m_feature->runMode(), m_feature->repeatCount());
    if (m_hasLoopTiming) {
        m_runStatusBar->setLoopTiming(m_loopNumber, m_loopElapsedMs, m_loopAverageMs, m_loopSuccess);
    } else {
        m_runStatusBar->clearLoopTiming();
    }
}

void WorkflowEditorPanel::setRunStatusButtonState(bool showStop,
                                                  bool enabled,
                                                  const QString& disabledToolTip) {
    if (m_runStatusBar) {
        m_runStatusBar->setRunButtonState(showStop, enabled, disabledToolTip);
    }
}

void WorkflowEditorPanel::setLoopTiming(int loopNumber, qint64 elapsedMs, qint64 averageMs, bool success) {
    m_hasLoopTiming = true;
    m_loopNumber = loopNumber;
    m_loopElapsedMs = elapsedMs;
    m_loopAverageMs = averageMs;
    m_loopSuccess = success;
    updateTitleText();
}

void WorkflowEditorPanel::clearLoopTiming() {
    m_hasLoopTiming = false;
    m_loopNumber = 0;
    m_loopElapsedMs = 0;
    m_loopAverageMs = 0;
    m_loopSuccess = true;
    updateTitleText();
}

void WorkflowEditorPanel::setProjectDirectory(const QString& directory, bool refreshList) {
    if (m_projectDirectory == directory) {
        return;
    }
    m_projectDirectory = directory;
    if (refreshList && m_feature) {
        refresh();
    }
}

void WorkflowEditorPanel::refresh() {
    PIPBONG_PERF_SCOPE("WorkflowEditorPanel.refresh");
    m_blockList->setBlockCount(0);
    m_blockList->clearLoopRegionVisuals();
    if (!m_feature) {
        m_blockList->setLoopRegions({});
        clearLoopTiming();
        updateTitleText();
        updateWorkflowToolButtonStates();
        setEditingEnabled(m_editingEnabled);
        return;
    }

    updateTitleText();

    const auto& blocks = m_feature->workflow().blocks();
    m_blockList->setBlockCount(static_cast<int>(blocks.size()));

    const bool roiCorrectionEligible = m_feature->roiCorrectionSessionEligible();
    const bool perBlockRoiCorrection = roiCorrectionEligible && !m_feature->roiCorrection();
    m_blockList->setRoiCorrectionColumnVisible(perBlockRoiCorrection);

    m_blockList->setLoopRegions(m_feature->workflow().loopRegions());

    QVector<int> lastMatchSources(static_cast<int>(blocks.size()), -1);
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        if (blocks[static_cast<size_t>(i)]->type() != BlockType::Click) {
            continue;
        }
        const auto& click = static_cast<const ClickBlock&>(*blocks[static_cast<size_t>(i)]);
        if (click.target != ClickBlock::ClickTarget::LastMatch) {
            continue;
        }
        lastMatchSources[i] = nearestPrecedingImageFindBlockIndex(blocks, i);
    }

    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const Block& block = *blocks[static_cast<size_t>(i)];
        QString summary = QString::fromStdString(block.listDetailSummary());
        if (lastMatchSources[i] >= 0) {
            summary.replace(QStringLiteral("·직전"),
                            QStringLiteral("·#%1").arg(lastMatchSources[i] + 1));
        }
        m_blockList->setBlockInfo(i,
                                  blockTypeWorkflowListName(block.type()),
                                  summary,
                                  loadBlockThumbnail(block, m_projectDirectory));

        if (block.type() != BlockType::ImageFind) {
            continue;
        }

        const auto& imageFind = static_cast<const ImageFindBlock&>(block);
        m_blockList->setBlockRoiCorrection(i, imageFind.roiCorrection, perBlockRoiCorrection);

        const double threshold = imageFind.threshold;
        if (i < static_cast<int>(m_rowMatchThresholds.size())) {
            m_rowMatchThresholds[i] = threshold;
        }

        const bool hasPeakScore = i < static_cast<int>(m_rowMatchConfidences.size())
                                  && m_rowMatchConfidences[i] >= 0.0;
        if (hasPeakScore) {
            const bool matched = (i < static_cast<int>(m_rowMatchMatched.size()) && m_rowMatchMatched[i])
                                 || m_blockList->isMatchSuccessLocked(i);
            const QPixmap image =
                i < static_cast<int>(m_rowMatchImages.size()) ? m_rowMatchImages[i] : QPixmap();
            m_blockList->setBlockMatchResult(
                i, threshold, m_rowMatchConfidences[i], image, matched);
        } else {
            m_blockList->setBlockMatchBaseline(i, threshold);
        }
    }

    m_blockList->setClickLastMatchSources(lastMatchSources);

    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        if (i < m_rowBlockDurations.size() && m_rowBlockDurations[i] >= 0) {
            m_blockList->setBlockDuration(i, m_rowBlockDurations[i]);
        }
        if (i < m_rowImageFindMatchDurations.size() && m_rowImageFindMatchDurations[i] >= 0) {
            m_blockList->setBlockImageFindMatchDuration(i, m_rowImageFindMatchDurations[i]);
        }
        if (i < m_rowImageFindAttemptCounts.size() && m_rowImageFindAttemptCounts[i] > 0) {
            m_blockList->setBlockImageFindAttemptCount(i, m_rowImageFindAttemptCounts[i]);
        }
        const int returnCount =
            i < m_rowImageFindReturnCounts.size() ? m_rowImageFindReturnCounts[i] : -1;
        const int retryCount =
            i < m_rowImageFindRetryCounts.size() ? m_rowImageFindRetryCounts[i] : -1;
        if (returnCount > 0 || retryCount > 0) {
            m_blockList->setBlockImageFindFailureHandlingCounts(
                i, qMax(0, returnCount), qMax(0, retryCount));
        }
    }
    if (m_activeBlockIndex >= 0 && m_activeBlockIndex < static_cast<int>(blocks.size())) {
        m_blockList->setActiveRow(m_activeBlockIndex, m_executionHighlight);
    }
    updateWorkflowToolButtonStates();
    updateClickBlockSelectionPreview();
    updateImageFindSelectionRoiPreview();
}

void WorkflowEditorPanel::updateWorkflowToolButtonStates() {
    bool hasWait = false;
    int blockCount = 0;
    if (m_feature) {
        const auto& blocks = m_feature->workflow().blocks();
        blockCount = static_cast<int>(blocks.size());
        for (const auto& block : blocks) {
            if (block->type() == BlockType::Wait) {
                hasWait = true;
                break;
            }
        }
    }

    const bool canEdit = m_editingEnabled && m_feature != nullptr;

    if (m_removeAllWaitButton) {
        m_removeAllWaitButton->setEnabled(canEdit && hasWait);
    }
    if (m_insertWaitBetweenButton) {
        m_insertWaitBetweenButton->setEnabled(canEdit && blockCount >= 2);
    }
    if (m_loopRegionsButton) {
        m_loopRegionsButton->setEnabled(canEdit && blockCount > 0);
    }
}

void WorkflowEditorPanel::setEditingEnabled(bool enabled) {
    m_editingEnabled = enabled;
    const bool canEdit = enabled && m_feature != nullptr;
    for (QPushButton* button : m_addTypeButtons) {
        button->setEnabled(canEdit);
    }
    updateWorkflowToolButtonStates();
    m_blockList->setReorderEnabled(canEdit);
    if (!enabled) {
        setLoopRegionPickMode(false);
    }
}

void WorkflowEditorPanel::addBlockOfType(BlockType type) {
    if (!m_feature) {
        return;
    }

    const std::string editingFeatureId = m_feature->id();
    auto draft = BlockFactory::create(type);
    if (type == BlockType::ImageFind) {
        if (auto* imageFind = dynamic_cast<ImageFindBlock*>(draft.get())) {
            imageFind->pollIntervalMs = lastImageFindPollIntervalMs();
        }
    }
    if (auto* mainWindow = qobject_cast<MainWindow*>(window())) {
        mainWindow->refreshCaptureTargetForEditing();
    }
    BlockEditorDialog dialog(draft.get(), m_projectDirectory, this);
    if (m_feature) {
        dialog.setWorkflowEditorContext(static_cast<int>(m_feature->workflow().blocks().size()) + 1,
                                        static_cast<int>(m_feature->workflow().blocks().size()));
        dialog.setRoiCorrectionUiPolicy(m_feature->roiCorrection(), m_feature->roiCorrectionSessionEligible());
        dialog.setClickFeatureRunOptions(m_feature->lockMouseToScreenCenterDuringRun(),
                                         m_feature->lockMouseToCurrentPositionDuringRun());
        wireBlockEditorContinuousInput(
            dialog, static_cast<int>(m_feature->workflow().blocks().size()));
    }
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (!m_feature || m_feature->id() != editingFeatureId) {
        return;
    }

    pushUndoSnapshot();
    auto newBlock = dialog.takeBlock();
    if (!newBlock) {
        return;
    }
    if (newBlock->type() == BlockType::Click) {
        m_feature->setLockMouseToScreenCenterDuringRun(dialog.lockMouseToScreenCenterDuringRun());
        m_feature->setLockMouseToCurrentPositionDuringRun(
            dialog.lockMouseToCurrentPositionDuringRun());
    }
    m_feature->workflow().addBlock(std::move(newBlock));
    refresh();
    m_blockList->selectBlockRow(static_cast<int>(m_feature->workflow().blocks().size()) - 1);
    emit workflowModified();
}

void WorkflowEditorPanel::onEditBlock() {
    const QList<int> rows = selectedBlockRows();
    if (rows.size() == 1) {
        editBlockAt(rows.first());
    }
}

QList<int> WorkflowEditorPanel::selectedBlockRows() const {
    QSet<int> rowSet;
    for (const QTableWidgetItem* item : m_blockList->selectedItems()) {
        const int tableRow = item->row();
        const int blockRow = m_blockList->blockRowForTableRow(tableRow);
        if (blockRow >= 0) {
            rowSet.insert(blockRow);
        }
    }
    QList<int> rows = rowSet.values();
    std::sort(rows.begin(), rows.end());
    return rows;
}

void WorkflowEditorPanel::selectBlockRows(const QList<int>& rows) {
    if (rows.isEmpty() || !m_blockList->selectionModel()) {
        return;
    }

    QItemSelectionModel* selectionModel = m_blockList->selectionModel();
    QSignalBlocker selectionBlocker(selectionModel);
    selectionModel->clearSelection();

    const int lastColumn = BlockListWidget::LastDataColumn;
    bool firstRow = true;
    int lastTableRow = -1;
    for (int blockRow : rows) {
        const int tableRow = m_blockList->tableRowForBlockRow(blockRow);
        if (tableRow < 0) {
            continue;
        }
        const QModelIndex topLeft = m_blockList->model()->index(tableRow, 0);
        const QModelIndex bottomRight = m_blockList->model()->index(tableRow, lastColumn);
        const QItemSelection rowSelection(topLeft, bottomRight);
        const QItemSelectionModel::SelectionFlags command =
            firstRow ? QItemSelectionModel::ClearAndSelect : QItemSelectionModel::Select;
        selectionModel->select(rowSelection, command | QItemSelectionModel::Rows);
        firstRow = false;
        lastTableRow = tableRow;
    }

    if (lastTableRow >= 0) {
        m_blockList->setCurrentCell(lastTableRow, 0);
        if (QTableWidgetItem* anchor = m_blockList->item(lastTableRow, 0)) {
            m_blockList->scrollToItem(anchor, QAbstractItemView::PositionAtCenter);
        }
    }
}

void WorkflowEditorPanel::onCopyBlocks() {
    if (!m_feature) {
        return;
    }

    const QList<int> rows = selectedBlockRows();
    if (rows.isEmpty()) {
        return;
    }

    m_clipboardBlocks.clear();
    const auto& blocks = m_feature->workflow().blocks();
    for (int row : rows) {
        if (row >= 0 && row < static_cast<int>(blocks.size())) {
            m_clipboardBlocks.push_back(blocks[row]->clone());
        }
    }

    m_blockList->flashClipboardCopyRange(rows.first(), rows.last());
    pulseClickBlocksAtRows(rows);
}

void WorkflowEditorPanel::onPasteBlocks() {
    if (!m_feature || m_clipboardBlocks.empty()) {
        return;
    }

    const QList<int> rows = selectedBlockRows();
    int insertAt = rows.isEmpty() ? static_cast<int>(m_feature->workflow().blocks().size())
                                  : rows.last() + 1;

    pushUndoSnapshot();
    QList<int> pastedRows;
    pastedRows.reserve(static_cast<int>(m_clipboardBlocks.size()));
    for (const auto& block : m_clipboardBlocks) {
        m_feature->workflow().insertBlock(insertAt, block->clone());
        pastedRows.append(insertAt);
        ++insertAt;
    }

    refresh();
    const QList<int> rowsToSelect = pastedRows;
    QTimer::singleShot(0, this, [this, rowsToSelect]() {
        selectBlockRows(rowsToSelect);
        updateClickBlockSelectionPreview();
    });
    m_blockList->flashClipboardPasteRange(pastedRows.first(), pastedRows.last());
    pulseClickBlocksAtRows(pastedRows);
    emit workflowModified();
}

void WorkflowEditorPanel::removeSelectedBlocks() {
    if (!m_feature) {
        return;
    }

    const QList<int> rows = selectedBlockRows();
    if (rows.isEmpty()) {
        return;
    }

    pushUndoSnapshot();

    for (int i = rows.size() - 1; i >= 0; --i) {
        m_feature->workflow().removeBlock(rows[i]);
    }

    clearBlockMatchResults();

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    refresh();

    if (blockCount > 0) {
        const int anchorRow = qMin(rows.first(), blockCount - 1);
        m_blockList->selectBlockRow(anchorRow);
    }

    emit workflowModified();
}

void WorkflowEditorPanel::onDeleteBlocks() {
    removeSelectedBlocks();
}

void WorkflowEditorPanel::onRemoveBlock() {
    removeSelectedBlocks();
}

void WorkflowEditorPanel::onRemoveAllWaitBlocks() {
    if (!m_feature) {
        return;
    }

    const auto& blocks = m_feature->workflow().blocks();
    QList<int> waitRows;
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        if (blocks[i]->type() == BlockType::Wait) {
            waitRows.append(i);
        }
    }
    if (waitRows.isEmpty()) {
        return;
    }

    pushUndoSnapshot();
    for (int i = waitRows.size() - 1; i >= 0; --i) {
        m_feature->workflow().removeBlock(waitRows[i]);
    }

    clearBlockMatchResults();
    clearExecutionHighlight();

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    refresh();

    if (blockCount > 0) {
        const int anchorRow = qMin(waitRows.first(), blockCount - 1);
        m_blockList->selectBlockRow(anchorRow);
    }

    emit workflowModified();
}

void WorkflowEditorPanel::onManageLoopRegions() {
    if (!m_feature || !m_editingEnabled) {
        return;
    }

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    if (blockCount <= 0) {
        return;
    }

    setLoopRegionPickMode(!m_loopRegionPickActive);
}

void WorkflowEditorPanel::setLoopRegionPickMode(bool active) {
    if (m_loopRegionPickActive == active) {
        return;
    }
    m_loopRegionPickActive = active;
    m_blockList->setLoopRegionPickMode(active);
    if (active) {
        stopClickBlockSelectionPreview();
        stopImageFindSelectionRoiPreview();
    } else {
        updateClickBlockSelectionPreview();
        updateImageFindSelectionRoiPreview();
    }
    if (m_loopRegionsButton) {
        m_loopRegionsButton->setChecked(active);
        m_loopRegionsButton->setText(active ? tr("구간 드래그…") : tr("구간 반복"));
    }
}

void WorkflowEditorPanel::onImageFindThresholdChanged(int blockRow, double threshold) {
    if (!m_feature || !m_editingEnabled) {
        return;
    }

    auto& blocks = m_feature->workflow().blocks();
    if (blockRow < 0 || blockRow >= static_cast<int>(blocks.size())) {
        return;
    }
    if (blocks[blockRow]->type() != BlockType::ImageFind) {
        return;
    }

    auto* imageFind = static_cast<ImageFindBlock*>(blocks[blockRow].get());
    if (qAbs(imageFind->threshold - threshold) < 0.0005) {
        return;
    }

    pushUndoSnapshot();
    imageFind->threshold = threshold;
    if (blockRow >= static_cast<int>(m_rowMatchThresholds.size())) {
        m_rowMatchThresholds.resize(blockRow + 1, 0.0);
    }
    m_rowMatchThresholds[blockRow] = threshold;

    const bool hasPeakScore = blockRow < static_cast<int>(m_rowMatchConfidences.size())
                              && m_rowMatchConfidences[blockRow] >= 0.0;
    if (hasPeakScore) {
        const bool matched = (blockRow < static_cast<int>(m_rowMatchMatched.size())
                              && m_rowMatchMatched[blockRow])
                             || m_blockList->isMatchSuccessLocked(blockRow);
        const QPixmap image = blockRow < static_cast<int>(m_rowMatchImages.size())
                                  ? m_rowMatchImages[blockRow]
                                  : QPixmap();
        m_blockList->setBlockMatchResult(blockRow,
                                         threshold,
                                         m_rowMatchConfidences[blockRow],
                                         image,
                                         matched);
    } else {
        m_blockList->setBlockMatchBaseline(blockRow, threshold);
    }
    emit workflowModified();
}

void WorkflowEditorPanel::onImageFindRoiCorrectionChanged(int blockRow, bool enabled) {
    if (!m_feature || !m_editingEnabled) {
        return;
    }

    auto& blocks = m_feature->workflow().blocks();
    if (blockRow < 0 || blockRow >= static_cast<int>(blocks.size())) {
        return;
    }
    if (blocks[blockRow]->type() != BlockType::ImageFind) {
        return;
    }

    auto* imageFind = static_cast<ImageFindBlock*>(blocks[blockRow].get());
    if (imageFind->roiCorrection == enabled) {
        return;
    }

    pushUndoSnapshot();
    imageFind->roiCorrection = enabled;
    const bool perBlockRoiCorrection =
        m_feature->roiCorrectionSessionEligible() && !m_feature->roiCorrection();
    m_blockList->setBlockRoiCorrection(blockRow, enabled, perBlockRoiCorrection);
    emit workflowModified();
}

void WorkflowEditorPanel::onLoopRegionPickCancelled() {
    setLoopRegionPickMode(false);
}

void WorkflowEditorPanel::editLoopRegion(const QString& regionId) {
    if (!m_feature || !m_editingEnabled || regionId.isEmpty()) {
        return;
    }

    std::vector<WorkflowLoopRegion> regions = m_feature->workflow().loopRegions();
    const auto it = std::find_if(regions.begin(), regions.end(), [&](const WorkflowLoopRegion& region) {
        return QString::fromStdString(region.id) == regionId;
    });
    if (it == regions.end()) {
        return;
    }

    WorkflowLoopRegionEditDialog dialog(static_cast<int>(m_feature->workflow().blocks().size()),
                                        &(*it),
                                        it->startIndex + 1,
                                        it->endIndex + 1,
                                        false,
                                        this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const WorkflowLoopRegion edited = dialog.resultRegion();
    if (loopRegionOverlaps(regions, edited.startIndex, edited.endIndex, edited.id)) {
        QMessageBox::warning(this,
                             tr("구간 반복"),
                             tr("편집한 구간이 다른 반복 구간과 겹칩니다."));
        return;
    }
    pushUndoSnapshot();
    *it = edited;
    m_feature->workflow().setLoopRegions(std::move(regions));
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::deleteLoopRegion(const QString& regionId) {
    if (!m_feature || !m_editingEnabled || regionId.isEmpty()) {
        return;
    }

    std::vector<WorkflowLoopRegion> regions = m_feature->workflow().loopRegions();
    const auto it = std::find_if(regions.begin(), regions.end(), [&](const WorkflowLoopRegion& region) {
        return QString::fromStdString(region.id) == regionId;
    });
    if (it == regions.end()) {
        return;
    }

    pushUndoSnapshot();
    regions.erase(it);
    m_feature->workflow().setLoopRegions(std::move(regions));
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::deleteBlockAt(int row) {
    if (!m_feature || !m_editingEnabled) {
        return;
    }
    if (row < 0 || row >= static_cast<int>(m_feature->workflow().blocks().size())) {
        return;
    }

    pushUndoSnapshot();
    m_feature->workflow().removeBlock(row);
    clearBlockMatchResults();
    refresh();

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    if (blockCount > 0) {
        m_blockList->selectBlockRow(qMin(row, blockCount - 1));
    }
    emit workflowModified();
}

void WorkflowEditorPanel::onLoopRegionEditRequested(const QString& regionId) {
    editLoopRegion(regionId);
}

void WorkflowEditorPanel::onLoopRegionDeleteRequested(const QString& regionId) {
    deleteLoopRegion(regionId);
}

void WorkflowEditorPanel::onLoopRegionsButtonContextMenu(const QPoint& pos) {
    QMenu menu(this);
    menu.addAction(tr("구간 반복 목록…"), this, &WorkflowEditorPanel::openLoopRegionsListDialog);
    menu.exec(m_loopRegionsButton->mapToGlobal(pos));
}

void WorkflowEditorPanel::openLoopRegionsListDialog() {
    if (!m_feature || !m_editingEnabled) {
        return;
    }

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    if (blockCount <= 0) {
        return;
    }

    pushUndoSnapshot();

    const QList<int> rows = selectedBlockRows();
    int defaultStart = 1;
    int defaultEnd = blockCount;
    if (!rows.isEmpty()) {
        defaultStart = rows.first() + 1;
        defaultEnd = rows.last() + 1;
    }

    WorkflowLoopRegionsDialog dialog(&m_feature->workflow(), defaultStart, defaultEnd, this);
    dialog.exec();
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::onLoopRegionRangePicked(int startRow, int endRow) {
    if (!m_feature || !m_editingEnabled) {
        return;
    }

    const int startIndex = qMin(startRow, endRow);
    const int endIndex = qMax(startRow, endRow);
    if (loopRegionOverlaps(m_feature->workflow().loopRegions(), startIndex, endIndex)) {
        QMessageBox::warning(this,
                             tr("구간 반복"),
                             tr("선택한 구간이 기존 반복 구간과 겹칩니다. 다른 범위를 선택하세요."));
        return;
    }

    WorkflowLoopRegionEditDialog dialog(static_cast<int>(m_feature->workflow().blocks().size()),
                                        nullptr,
                                        startIndex + 1,
                                        endIndex + 1,
                                        true,
                                        this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    pushUndoSnapshot();
    std::vector<WorkflowLoopRegion> regions = m_feature->workflow().loopRegions();
    regions.push_back(dialog.resultRegion());
    m_feature->workflow().setLoopRegions(std::move(regions));
    refresh();
    emit workflowModified();
    setLoopRegionPickMode(false);
}

void WorkflowEditorPanel::onInsertWaitBetweenBlocks() {
    if (!m_feature) {
        return;
    }

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    if (blockCount < 2) {
        return;
    }

    bool accepted = false;
    const int waitMs = snapWaitDelayMs(QInputDialog::getInt(this,
                                            tr("전체 딜레이 추가"),
                                            tr("삽입할 딜레이 시간 (ms):"),
                                            lastBulkInsertWaitMs(),
                                            0,
                                            600000,
                                            kWaitDelayStepMs,
                                            &accepted));
    if (!accepted) {
        return;
    }

    saveLastBulkInsertWaitMs(waitMs);

    pushUndoSnapshot();
    for (int i = blockCount - 2; i >= 0; --i) {
        auto waitBlock = BlockFactory::create(BlockType::Wait);
        static_cast<WaitBlock*>(waitBlock.get())->ms = waitMs;
        m_feature->workflow().insertBlock(i + 1, std::move(waitBlock));
    }

    clearBlockMatchResults();
    clearExecutionHighlight();
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::onMoveUp() {
    if (!m_feature) {
        return;
    }
    const int row = m_blockList->currentRow();
    if (row <= 0) {
        return;
    }
    pushUndoSnapshot();
    m_feature->workflow().moveBlockUp(row);
    refresh();
    m_blockList->selectBlockRow(row - 1);
    emit workflowModified();
}

void WorkflowEditorPanel::onMoveDown() {
    if (!m_feature) {
        return;
    }
    const int row = m_blockList->currentRow();
    if (row < 0 || row + 1 >= static_cast<int>(m_feature->workflow().blocks().size())) {
        return;
    }
    pushUndoSnapshot();
    m_feature->workflow().moveBlockDown(row);
    refresh();
    m_blockList->selectBlockRow(row + 1);
    emit workflowModified();
}

void WorkflowEditorPanel::onBlockRowsReordered(int fromRow, int toRow) {
    if (!m_feature) {
        return;
    }
    if (fromRow != toRow) {
        pushUndoSnapshot();
        m_feature->workflow().moveBlock(fromRow, toRow);
        emit workflowModified();
    }
    refresh();
    if (fromRow != toRow) {
        m_blockList->selectBlockRow(toRow);
    } else if (fromRow >= 0) {
        m_blockList->selectBlockRow(fromRow);
    }
}

void WorkflowEditorPanel::onBlockDoubleClicked(int row) {
    editBlockAt(row);
}

bool WorkflowEditorPanel::editBlockAt(int row) {
    if (!m_feature || row < 0 || row >= static_cast<int>(m_feature->workflow().blocks().size())) {
        return false;
    }

    const std::string editingFeatureId = m_feature->id();
    Block* block = m_feature->workflow().blocks()[row].get();
    if (auto* mainWindow = qobject_cast<MainWindow*>(window())) {
        mainWindow->refreshCaptureTargetForEditing();
    }
    stopImageFindSelectionRoiPreview();
    BlockEditorDialog dialog(block, m_projectDirectory, this);
    dialog.setWorkflowEditorContext(static_cast<int>(m_feature->workflow().blocks().size()), row);
    dialog.setRoiCorrectionUiPolicy(m_feature->roiCorrection(), m_feature->roiCorrectionSessionEligible());
    dialog.setClickFeatureRunOptions(m_feature->lockMouseToScreenCenterDuringRun(),
                                     m_feature->lockMouseToCurrentPositionDuringRun());
    wireBlockEditorContinuousInput(dialog, row + 1);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    if (!m_feature || m_feature->id() != editingFeatureId
        || row < 0 || row >= static_cast<int>(m_feature->workflow().blocks().size())) {
        return false;
    }

    pushUndoSnapshot();
    auto updatedBlock = dialog.takeBlock();
    if (!updatedBlock) {
        return false;
    }
    if (updatedBlock->type() == BlockType::Click) {
        m_feature->setLockMouseToScreenCenterDuringRun(dialog.lockMouseToScreenCenterDuringRun());
        m_feature->setLockMouseToCurrentPositionDuringRun(
            dialog.lockMouseToCurrentPositionDuringRun());
    }
    m_feature->workflow().replaceBlock(row, std::move(updatedBlock));
    refresh();
    m_blockList->selectBlockRow(row);
    emit workflowModified();
    return true;
}

void WorkflowEditorPanel::clearWorkflowHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void WorkflowEditorPanel::pushUndoSnapshot() {
    if (!m_feature) {
        return;
    }

    m_undoStack.push_back(m_feature->workflow().clone());
    if (static_cast<int>(m_undoStack.size()) > kMaxUndoDepth) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();
}

void WorkflowEditorPanel::wireBlockEditorContinuousInput(BlockEditorDialog& dialog,
                                                         int initialInsertAt) {
    if (!m_feature) {
        return;
    }

    struct ContinuousInsertState {
        int insertAt = 0;
        bool undoPrepared = false;
    };
    auto state = std::make_shared<ContinuousInsertState>();
    state->insertAt = qMax(0, initialInsertAt);

    dialog.setContinuousClickInsertHandler([this, state](std::unique_ptr<Block> block) {
        if (!m_feature || !block) {
            return;
        }
        if (!state->undoPrepared) {
            pushUndoSnapshot();
            state->undoPrepared = true;
        }
        const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
        const int index = qBound(0, state->insertAt, blockCount);
        m_feature->workflow().insertBlock(index, std::move(block));
        ++state->insertAt;
        refresh();
        m_blockList->selectBlockRow(state->insertAt - 1);
        emit workflowModified();
    });
}

void WorkflowEditorPanel::restoreFromSnapshot(const Workflow& snapshot) {
    if (!m_feature) {
        return;
    }

    m_feature->workflow().assignFrom(snapshot);
    clearBlockMatchResults();
    clearExecutionHighlight();
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::onUndo() {
    if (!m_feature || m_undoStack.empty()) {
        return;
    }

    m_redoStack.push_back(m_feature->workflow().clone());
    if (static_cast<int>(m_redoStack.size()) > kMaxUndoDepth) {
        m_redoStack.erase(m_redoStack.begin());
    }

    std::unique_ptr<Workflow> snapshot = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    restoreFromSnapshot(*snapshot);
}

void WorkflowEditorPanel::onRedo() {
    if (!m_feature || m_redoStack.empty()) {
        return;
    }

    m_undoStack.push_back(m_feature->workflow().clone());
    if (static_cast<int>(m_undoStack.size()) > kMaxUndoDepth) {
        m_undoStack.erase(m_undoStack.begin());
    }

    std::unique_ptr<Workflow> snapshot = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    restoreFromSnapshot(*snapshot);
}

void WorkflowEditorPanel::stopClickBlockSelectionPreview() {
    m_clickSelectionPreviewBlockRow = -1;
    if (m_clickSelectionPreviewTimer) {
        m_clickSelectionPreviewTimer->stop();
    }
}

void WorkflowEditorPanel::stopImageFindSelectionRoiPreview() {
    m_imageFindSelectionRoiBlockRow = -1;
    if (m_imageFindSelectionRoiTimer) {
        m_imageFindSelectionRoiTimer->stop();
    }
    WorkflowImageFindSelectionRoiOverlay::dismissAll();
}

void WorkflowEditorPanel::updateImageFindSelectionRoiPreview() {
    if (!m_imageFindSelectionRoiTimer) {
        return;
    }
    if (m_activeBlockIndex >= 0 && m_executionHighlight != BlockListWidget::ExecutionHighlight::None) {
        stopImageFindSelectionRoiPreview();
        return;
    }
    if (m_loopRegionPickActive) {
        stopImageFindSelectionRoiPreview();
        return;
    }

    const QList<int> rows = selectedBlockRows();
    if (!m_feature || rows.size() != 1) {
        stopImageFindSelectionRoiPreview();
        return;
    }

    const int blockRow = rows.first();
    const auto& blocks = m_feature->workflow().blocks();
    if (blockRow < 0 || blockRow >= static_cast<int>(blocks.size())
        || blocks[static_cast<size_t>(blockRow)]->type() != BlockType::ImageFind) {
        stopImageFindSelectionRoiPreview();
        return;
    }

    if (m_imageFindSelectionRoiBlockRow != blockRow) {
        m_imageFindSelectionRoiBlockRow = blockRow;
        onImageFindSelectionRoiPreviewTick();
    }

    constexpr int kRefreshMs = 280;
    if (!m_imageFindSelectionRoiTimer->isActive()) {
        m_imageFindSelectionRoiTimer->start(kRefreshMs);
    } else {
        m_imageFindSelectionRoiTimer->setInterval(kRefreshMs);
    }
}

void WorkflowEditorPanel::onImageFindSelectionRoiPreviewTick() {
    if (!m_feature || m_imageFindSelectionRoiBlockRow < 0) {
        return;
    }
    if (m_activeBlockIndex >= 0 && m_executionHighlight != BlockListWidget::ExecutionHighlight::None) {
        return;
    }

    const auto& blocks = m_feature->workflow().blocks();
    const int blockRow = m_imageFindSelectionRoiBlockRow;
    if (blockRow < 0 || blockRow >= static_cast<int>(blocks.size())
        || blocks[static_cast<size_t>(blockRow)]->type() != BlockType::ImageFind) {
        return;
    }

    const auto& imageFind = static_cast<const ImageFindBlock&>(*blocks[static_cast<size_t>(blockRow)]);
    showImageFindSelectionRoiOverlay(imageFind);
}

void WorkflowEditorPanel::updateClickBlockSelectionPreview() {
    if (!m_clickSelectionPreviewTimer) {
        return;
    }
    if (m_activeBlockIndex >= 0 && m_executionHighlight != BlockListWidget::ExecutionHighlight::None) {
        stopClickBlockSelectionPreview();
        return;
    }
    if (m_loopRegionPickActive) {
        stopClickBlockSelectionPreview();
        return;
    }

    const QList<int> rows = selectedBlockRows();
    if (!m_feature || rows.size() != 1) {
        stopClickBlockSelectionPreview();
        return;
    }

    const int blockRow = rows.first();
    const auto& blocks = m_feature->workflow().blocks();
    if (blockRow < 0 || blockRow >= static_cast<int>(blocks.size())
        || blocks[static_cast<size_t>(blockRow)]->type() != BlockType::Click) {
        stopClickBlockSelectionPreview();
        return;
    }

    if (m_clickSelectionPreviewBlockRow != blockRow) {
        m_clickSelectionPreviewBlockRow = blockRow;
        onClickBlockSelectionPreviewTick();
    }

    const int intervalMs =
        qMax(120, static_cast<int>(PointerFeedbackSettings::click().displayDurationMs * 0.85));
    if (!m_clickSelectionPreviewTimer->isActive()) {
        m_clickSelectionPreviewTimer->start(intervalMs);
    } else {
        m_clickSelectionPreviewTimer->setInterval(intervalMs);
    }
}

void WorkflowEditorPanel::onBlockSelectionChanged() {
    updateClickBlockSelectionPreview();
    updateImageFindSelectionRoiPreview();
}

void WorkflowEditorPanel::onClickBlockSelectionPreviewTick() {
    if (!m_feature || m_clickSelectionPreviewBlockRow < 0) {
        return;
    }
    if (m_activeBlockIndex >= 0 && m_executionHighlight != BlockListWidget::ExecutionHighlight::None) {
        return;
    }

    const auto& blocks = m_feature->workflow().blocks();
    const int blockRow = m_clickSelectionPreviewBlockRow;
    if (blockRow < 0 || blockRow >= static_cast<int>(blocks.size())
        || blocks[static_cast<size_t>(blockRow)]->type() != BlockType::Click) {
        return;
    }

    const auto& click = static_cast<const ClickBlock&>(*blocks[static_cast<size_t>(blockRow)]);
    int clientX = 0;
    int clientY = 0;
    if (!resolveClickBlockPreviewClientPoint(click,
                                             blockRow,
                                             m_feature->workflow(),
                                             m_rowMatchMatched,
                                             m_rowMatchHasClientPoint,
                                             m_rowMatchClientX,
                                             m_rowMatchClientY,
                                             clientX,
                                             clientY)) {
        return;
    }

    WorkflowMatchFeedbackOverlay::pulseAtClientPoint(
        clientX, clientY, RunPointerFeedbackKind::Click, PointerFeedbackSettings::click());
}

void WorkflowEditorPanel::pulseClickBlocksAtRows(const QList<int>& blockRows) {
#ifdef _WIN32
    if (!m_feature || blockRows.isEmpty()) {
        return;
    }
    const auto& blocks = m_feature->workflow().blocks();
    for (int blockRow : blockRows) {
        if (blockRow < 0 || blockRow >= static_cast<int>(blocks.size())) {
            continue;
        }
        if (blocks[static_cast<size_t>(blockRow)]->type() != BlockType::Click) {
            continue;
        }
        const auto& click = static_cast<const ClickBlock&>(*blocks[static_cast<size_t>(blockRow)]);
        int clientX = 0;
        int clientY = 0;
        if (!resolveClickBlockPreviewClientPoint(click,
                                                 blockRow,
                                                 m_feature->workflow(),
                                                 m_rowMatchMatched,
                                                 m_rowMatchHasClientPoint,
                                                 m_rowMatchClientX,
                                                 m_rowMatchClientY,
                                                 clientX,
                                                 clientY)) {
            continue;
        }
        WorkflowMatchFeedbackOverlay::pulseAtClientPoint(
            clientX, clientY, RunPointerFeedbackKind::Click, PointerFeedbackSettings::click());
    }
#else
    (void)blockRows;
#endif
}
