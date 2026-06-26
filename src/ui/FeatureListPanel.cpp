#include "ui/FeatureListPanel.h"
#include "ui/FeatureListWidget.h"
#include "ui/HotkeyBindingIcon.h"
#include "model/Feature.h"
#include "model/FeatureRunMode.h"
#include "model/Project.h"
#include "core/input/HotkeyBinding.h"
#include "ui/editors/FeatureEditDialog.h"
#include "ui/UiThemeColors.h"
#include <QApplication>
#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
namespace {
constexpr int kHeaderExtraHeight = 4;
constexpr int kRowSeparatorHeight = 1;
constexpr int kSelectionBarWidth = 3;
constexpr int kFeatureIdRole = Qt::UserRole;
constexpr int kFeatureNameRole = Qt::UserRole + 1;
constexpr int kRunModeDisplayRole = Qt::UserRole + 2;
constexpr int kHotkeyVirtualKeyRole = Qt::UserRole + 4;
constexpr int kHotkeyCtrlRole = Qt::UserRole + 5;
constexpr int kHotkeyAltRole = Qt::UserRole + 6;
constexpr int kHotkeyShiftRole = Qt::UserRole + 7;
constexpr int kColumnMargin = 6;
constexpr int kColumnGap = 4;
constexpr int kMinModeColumnWidth = 28;
constexpr int kMaxModeColumnWidth = 120;
constexpr int kMinHotkeyColumnWidth = 36;
constexpr int kMaxHotkeyColumnWidth = 180;
constexpr int kMinRowHeight = 20;
constexpr int kMaxRowHeight = 48;
constexpr int kResizeHandlePixels = 5;
constexpr int kRunButtonColumnWidth = 24;
struct FeatureListColumnRects {
    QRect run;
    QRect name;
    QRect mode;
    QRect hotkey;
};
struct FeatureListColumnEdges {
    FeatureListColumnRects cols;
    int modeDividerX = 0;
    int hotkeyDividerX = 0;
};
FeatureListColumnEdges featureListColumnEdges(const QRect& itemRect, const FeatureListColumnLayout& layout) {
    FeatureListColumnEdges edges;
    const QRect content = itemRect.adjusted(kColumnMargin, 0, -kColumnMargin, 0);

    const int runLeft = content.left();
    const int nameLeft = runLeft + kRunButtonColumnWidth + kColumnGap;
    const int hotkeyLeft = content.right() - layout.hotkeyColumnWidth + 1;
    const int modeLeft = hotkeyLeft - kColumnGap - layout.modeColumnWidth;
    const int nameWidth = qMax(0, modeLeft - kColumnGap - nameLeft);

    edges.hotkeyDividerX = hotkeyLeft;
    edges.modeDividerX = modeLeft;
    edges.cols.run = QRect(runLeft, content.top(), kRunButtonColumnWidth, content.height());
    edges.cols.hotkey = QRect(hotkeyLeft, content.top(), layout.hotkeyColumnWidth, content.height());
    edges.cols.mode = QRect(modeLeft, content.top(), layout.modeColumnWidth, content.height());
    edges.cols.name = QRect(nameLeft, content.top(), nameWidth, content.height());
    return edges;
}
FeatureListColumnRects featureListColumnRects(const QRect& itemRect, const FeatureListColumnLayout& layout) {
    return featureListColumnEdges(itemRect, layout).cols;
}
QString featureRunModeLabel(FeatureRunMode mode) {
    switch (normalizeRunMode(mode)) {
    case FeatureRunMode::Hold:
        return QObject::tr("누를 동안");
    case FeatureRunMode::RepeatInfinite:
        return QObject::tr("무한");
    case FeatureRunMode::RepeatCount:
        return QObject::tr("N회");
    case FeatureRunMode::Toggle:
    default:
        return QObject::tr("N회");
    }
}
QString featureRunModeCompact(FeatureRunMode mode, int repeatCount) {
    switch (normalizeRunMode(mode)) {
    case FeatureRunMode::Hold:
        return QObject::tr("홀드");
    case FeatureRunMode::RepeatInfinite:
        return QStringLiteral("\u221e");
    case FeatureRunMode::RepeatCount:
        return repeatCount <= 1 ? QStringLiteral("\u00d71") : QStringLiteral("\u00d7%1").arg(repeatCount);
    case FeatureRunMode::Toggle:
    default:
        return QStringLiteral("\u00d71");
    }
}

HotkeyBinding hotkeyBindingFromIndex(const QModelIndex& index) {
    HotkeyBinding binding;
    binding.virtualKey = index.data(kHotkeyVirtualKeyRole).toInt();
    binding.ctrl = index.data(kHotkeyCtrlRole).toBool();
    binding.alt = index.data(kHotkeyAltRole).toBool();
    binding.shift = index.data(kHotkeyShiftRole).toBool();
    return binding;
}

bool darkThemeFromPalette(const QPalette& pal) {
    const QColor window = pal.color(QPalette::Window);
    if (window.isValid()) {
        return window.lightness() < 128;
    }
    return pal.color(QPalette::WindowText).lightness() > 128;
}

QColor featureListHeaderTextColor(const QPalette& pal) {
    const QColor window = pal.color(QPalette::Window);
    const QColor windowText = pal.color(QPalette::WindowText);
    const QColor highlight = pal.color(QPalette::Highlight);
    const bool darkTheme = darkThemeFromPalette(pal);

    if (highlight.isValid() && highlight != windowText) {
        if (darkTheme) {
            QColor accent = highlight.lighter(155);
            if (accent.lightness() < 125) {
                accent = QColor(0x8e, 0xc5, 0xff);
            }
            return accent;
        }
        QColor accent = highlight.darker(120);
        if (accent.lightness() > 165 || accent.lightness() < 50) {
            accent = QColor(0x1e, 0x5a, 0x8e);
        }
        return accent;
    }

    if (darkTheme) {
        return QColor(0xb8, 0xc4, 0xd4);
    }
    return QColor(0x2f, 0x5d, 0x8a);
}

QColor featureListMutedTextColor(const QPalette& pal) {
    return secondaryHintTextColor(pal);
}

QString elideCellText(const QFontMetrics& metrics, const QString& text, int width) {
    return metrics.elidedText(text, Qt::ElideMiddle, qMax(width - 4, 0));
}

void drawCellText(QPainter* painter,
                  const QRect& rect,
                  const QString& text,
                  const QFont& font,
                  const QColor& color) {
    painter->setFont(font);
    painter->setPen(color);
    const QFontMetrics metrics(font);
    painter->drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, elideCellText(metrics, text, rect.width()));
}

QRect featureRunButtonHitRect(const QRect& runColumnRect) {
    const int size = qMin(runColumnRect.width(), runColumnRect.height()) - 4;
    return QRect(runColumnRect.left() + (runColumnRect.width() - size) / 2,
                 runColumnRect.top() + (runColumnRect.height() - size) / 2,
                 size,
                 size);
}

void paintFeatureRunButton(QPainter* painter,
                           const QRect& runColumnRect,
                           bool showStop,
                           bool enabled,
                           const QPalette& palette) {
    const QRect btnRect = featureRunButtonHitRect(runColumnRect);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor fill = enabled ? palette.color(QPalette::Highlight) : palette.color(QPalette::Mid);
    if (!enabled) {
        fill.setAlpha(120);
    } else if (showStop) {
        fill = fill.darker(108);
    }

    painter->setPen(QPen(enabled ? fill.darker(125) : palette.color(QPalette::Dark), 1.0));
    painter->setBrush(fill);
    painter->drawRoundedRect(btnRect, 4, 4);

    painter->setPen(Qt::NoPen);
    painter->setBrush(enabled ? Qt::white : palette.color(QPalette::Light));
    if (showStop) {
        const int pad = qMax(3, btnRect.width() / 4);
        painter->drawRoundedRect(btnRect.adjusted(pad, pad, -pad, -pad), 1, 1);
    } else {
        QPainterPath play;
        const qreal left = btnRect.left() + btnRect.width() * 0.38;
        const qreal right = btnRect.right() - btnRect.width() * 0.28;
        const qreal cy = btnRect.center().y();
        const qreal halfH = btnRect.height() * 0.22;
        play.moveTo(left, cy - halfH);
        play.lineTo(right, cy);
        play.lineTo(left, cy + halfH);
        play.closeSubpath();
        painter->drawPath(play);
    }
    painter->restore();
}

void paintFeatureListRowChrome(QPainter* painter,
                               const QRect& rect,
                               bool selected,
                               bool running,
                               const QPalette& palette) {
    QColor rowBg = palette.color(QPalette::Base);
    if (selected) {
        const QColor highlight = palette.color(QPalette::Highlight);
        rowBg = QColor(rowBg.red() + (highlight.red() - rowBg.red()) / 6,
                       rowBg.green() + (highlight.green() - rowBg.green()) / 6,
                       rowBg.blue() + (highlight.blue() - rowBg.blue()) / 6);
    }
    painter->fillRect(rect, rowBg);

    if (selected) {
        painter->fillRect(QRect(rect.left(), rect.top(), kSelectionBarWidth, rect.height()),
                          palette.color(QPalette::Highlight));
    } else if (running) {
        painter->fillRect(QRect(rect.left(), rect.top(), kSelectionBarWidth, rect.height()),
                          palette.color(QPalette::Highlight));
    }

    QColor separator = palette.color(QPalette::Mid);
    if (darkThemeFromPalette(palette) && separator.lightness() < 90) {
        separator = separator.lighter(130);
    }
    separator.setAlpha(80);
    painter->fillRect(QRect(rect.left(), rect.bottom(), rect.width(), kRowSeparatorHeight), separator);
}

void paintFeatureName(QPainter* painter,
                      const QRect& rect,
                      const QStyleOptionViewItem& option,
                      const QString& name,
                      bool runningGlow,
                      int animationPhase) {
    QFont nameFont = option.font;
    if (runningGlow) {
        nameFont.setBold(true);
    }
    const QFontMetrics metrics(nameFont);
    const QString elided = elideCellText(metrics, name, rect.width());
    const Qt::Alignment align = Qt::AlignHCenter | Qt::AlignVCenter;
    const QColor baseText = option.palette.color(
        (option.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text);
    if (!runningGlow) {
        painter->setFont(nameFont);
        painter->setPen(baseText);
        painter->drawText(rect, align, elided);
        return;
    }
    const int hue = (animationPhase * 5) % 360;
    QColor nameColor = QColor::fromHsv(hue, 210, 255);
    if (option.state & QStyle::State_Selected) {
        nameColor = QColor::fromHsv(hue, 170, 255);
    }
    for (int offset = 2; offset >= 0; --offset) {
        QColor glow = nameColor;
        glow.setAlpha(70 - offset * 20);
        painter->setPen(glow);
        painter->setFont(nameFont);
        painter->drawText(rect.adjusted(-offset, 0, offset, 0), align, elided);
    }
    painter->setPen(nameColor);
    painter->setFont(nameFont);
    painter->drawText(rect, align, elided);
}

void paintFeatureListHeaderChrome(QPainter* painter, const QRect& rect, const QPalette& pal) {
    QColor headerBg = pal.color(QPalette::Button);
    if (!headerBg.isValid()) {
        headerBg = pal.color(QPalette::AlternateBase);
    }
    if (darkThemeFromPalette(pal)) {
        headerBg = headerBg.darker(108);
    } else {
        headerBg = headerBg.lighter(102);
    }
    painter->fillRect(rect, headerBg);

    QColor bottomLine = pal.color(QPalette::Mid);
    if (darkThemeFromPalette(pal) && bottomLine.lightness() < 90) {
        bottomLine = bottomLine.lighter(140);
    }
    painter->setPen(QPen(bottomLine, 1));
    painter->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
}

void paintFeatureListColumnGuides(QPainter* painter,
                                  const FeatureListColumnEdges& edges,
                                  int height,
                                  const QPalette& pal) {
    QColor guideColor = pal.color(QPalette::Mid);
    if (darkThemeFromPalette(pal) && guideColor.lightness() < 90) {
        guideColor = guideColor.lighter(140);
    }
    guideColor.setAlpha(70);
    painter->setPen(QPen(guideColor, 1));
    painter->drawLine(edges.modeDividerX, 0, edges.modeDividerX, height);
    painter->drawLine(edges.hotkeyDividerX, 0, edges.hotkeyDividerX, height);
}

void applyFeatureListPanelStyle(QWidget* panel) {
    panel->setObjectName(QStringLiteral("featureListPanel"));
    panel->setStyleSheet(QStringLiteral(
        "QWidget#featureListPanel {"
        "  background: transparent;"
        "}"
        "QGroupBox#featureListGroup {"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  margin-top: 10px;"
        "  padding: 8px;"
        "  padding-top: 18px;"
        "}"
        "QGroupBox#featureListGroup::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 10px;"
        "  padding: 0 6px;"
        "}"
        "QFrame#featureListTableFrame {"
        "  background-color: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "}"
        "QListWidget#featureListView {"
        "  background: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget#featureListView::item {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0;"
        "}"
        "QListWidget#featureListView::item:selected {"
        "  background: transparent;"
        "}"
        "QPushButton.featureListToolButton {"
        "  padding: 5px 12px;"
        "  border: 1px solid palette(mid);"
        "  border-radius: 5px;"
        "  background-color: palette(button);"
        "}"
        "QPushButton.featureListToolButton:hover {"
        "  background-color: palette(light);"
        "}"
        "QPushButton.featureListToolButton:pressed {"
        "  background-color: palette(midlight);"
        "}"));
}
enum class FeatureListResizeHandle {
    None,
    ModeColumnWidth,
    HotkeyColumnWidth,
    RowHeight,
};
class FeatureListHeaderWidget : public QWidget {
public:
    explicit FeatureListHeaderWidget(FeatureListPanel* panel)
        : QWidget(panel)
        , m_panel(panel) {
        setObjectName(QStringLiteral("featureListHeaderRow"));
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        updateHeight();
    }
    QSize sizeHint() const override {
        const int rowHeight = m_panel ? m_panel->columnLayout().rowHeight : 28;
        return {200, rowHeight + kHeaderExtraHeight};
    }
    void syncToLayout() {
        updateHeight();
        update();
    }
protected:
    void changeEvent(QEvent* event) override {
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
            update();
        }
        QWidget::changeEvent(event);
    }
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        if (!m_panel) {
            return;
        }
        const FeatureListColumnLayout layout = m_panel->columnLayout();
        const FeatureListColumnEdges edges = featureListColumnEdges(rect(), layout);
        const FeatureListColumnRects& cols = edges.cols;
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        const QPalette pal = palette();
        paintFeatureListHeaderChrome(&painter, rect(), pal);

        QFont headerFont = font();
        headerFont.setPointSize(qMax(headerFont.pointSize(), 9));
        headerFont.setBold(true);
        painter.setFont(headerFont);
        painter.setPen(featureListHeaderTextColor(pal));
        const Qt::Alignment headerAlign = Qt::AlignHCenter | Qt::AlignVCenter;
        painter.drawText(cols.name, headerAlign, tr("기능"));
        painter.drawText(cols.mode, headerAlign, tr("방식"));
        painter.drawText(cols.hotkey, headerAlign, tr("키"));

        paintFeatureListColumnGuides(&painter, edges, height(), pal);

        if (m_activeHandle != FeatureListResizeHandle::None) {
            painter.setPen(QPen(palette().color(QPalette::Highlight), 1, Qt::DashLine));
            if (m_activeHandle == FeatureListResizeHandle::RowHeight) {
                painter.drawLine(0, m_dragGuideY, width(), m_dragGuideY);
            } else {
                painter.drawLine(m_dragGuideX, 0, m_dragGuideX, height());
            }
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (!m_panel) {
            return;
        }
        if (m_activeHandle != FeatureListResizeHandle::None) {
            applyDrag(event->position().toPoint());
            return;
        }
        setCursor(cursorForHandle(handleAt(event->position().toPoint())));
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (!m_panel || event->button() != Qt::LeftButton) {
            return;
        }
        m_activeHandle = handleAt(event->position().toPoint());
        if (m_activeHandle == FeatureListResizeHandle::None) {
            return;
        }
        m_dragStartPos = event->position().toPoint();
        m_dragStartLayout = m_panel->columnLayout();
        m_dragGuideX = event->position().toPoint().x();
        m_dragGuideY = event->position().toPoint().y();
        grabMouse();
        setCursor(cursorForHandle(m_activeHandle));
        update();
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        Q_UNUSED(event);
        if (m_activeHandle == FeatureListResizeHandle::None) {
            return;
        }
        m_activeHandle = FeatureListResizeHandle::None;
        if (mouseGrabber() == this) {
            releaseMouse();
        }
        unsetCursor();
        update();
    }
private:
    FeatureListResizeHandle handleAt(const QPoint& pos) const {
        if (!m_panel) {
            return FeatureListResizeHandle::None;
        }
        if (pos.y() >= height() - kResizeHandlePixels) {
            return FeatureListResizeHandle::RowHeight;
        }

        const FeatureListColumnEdges edges =
            featureListColumnEdges(rect(), m_panel->columnLayout());
        const int modeDist = qAbs(pos.x() - edges.modeDividerX);
        const int hotkeyDist = qAbs(pos.x() - edges.hotkeyDividerX);

        if (modeDist <= kResizeHandlePixels && modeDist <= hotkeyDist) {
            return FeatureListResizeHandle::ModeColumnWidth;
        }
        if (hotkeyDist <= kResizeHandlePixels) {
            return FeatureListResizeHandle::HotkeyColumnWidth;
        }
        return FeatureListResizeHandle::None;
    }
    static Qt::CursorShape cursorForHandle(FeatureListResizeHandle handle) {
        switch (handle) {
        case FeatureListResizeHandle::RowHeight:
            return Qt::SizeVerCursor;
        case FeatureListResizeHandle::None:
            return Qt::ArrowCursor;
        default:
            return Qt::SplitHCursor;
        }
    }
    void applyDrag(const QPoint& pos) {
        const int deltaX = pos.x() - m_dragStartPos.x();
        const int deltaY = pos.y() - m_dragStartPos.y();
        FeatureListColumnLayout layout = m_dragStartLayout;
        switch (m_activeHandle) {
        case FeatureListResizeHandle::ModeColumnWidth:
            layout.modeColumnWidth = qBound(kMinModeColumnWidth,
                                            m_dragStartLayout.modeColumnWidth - deltaX,
                                            kMaxModeColumnWidth);
            m_dragGuideX = pos.x();
            break;
        case FeatureListResizeHandle::HotkeyColumnWidth:
            layout.hotkeyColumnWidth = qBound(kMinHotkeyColumnWidth,
                                              m_dragStartLayout.hotkeyColumnWidth - deltaX,
                                              kMaxHotkeyColumnWidth);
            m_dragGuideX = pos.x();
            break;
        case FeatureListResizeHandle::RowHeight:
            layout.rowHeight =
                qBound(kMinRowHeight, m_dragStartLayout.rowHeight + deltaY, kMaxRowHeight);
            m_dragGuideY = pos.y();
            break;
        default:
            return;
        }
        m_panel->setColumnLayout(layout, true);
    }
    void updateHeight() {
        const int rowHeight = m_panel ? m_panel->columnLayout().rowHeight : 28;
        const int h = rowHeight + kHeaderExtraHeight;
        setMinimumHeight(h);
        setMaximumHeight(h);
        updateGeometry();
    }
    FeatureListPanel* m_panel = nullptr;
    FeatureListResizeHandle m_activeHandle = FeatureListResizeHandle::None;
    QPoint m_dragStartPos;
    FeatureListColumnLayout m_dragStartLayout;
    int m_dragGuideX = 0;
    int m_dragGuideY = 0;
};
class FeatureListItemDelegate : public QStyledItemDelegate {
public:
    explicit FeatureListItemDelegate(const FeatureListPanel* panel)
        : m_panel(panel) {}
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        const int rowHeight = m_panel ? m_panel->columnLayout().rowHeight : 26;
        return {option.rect.width(), rowHeight};
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!m_panel || !index.isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const FeatureListColumnRects cols = featureListColumnRects(opt.rect, m_panel->columnLayout());
        const QString featureId = index.data(kFeatureIdRole).toString();
        const QString featureName = index.data(kFeatureNameRole).toString();
        const QString modeText = index.data(kRunModeDisplayRole).toString();
        const HotkeyBinding hotkeyBinding = hotkeyBindingFromIndex(index);
        const bool hasHotkey = !hotkeyBinding.isEmpty();
        const bool isRunning = m_panel->isFeatureRunning(featureId);
        const FeatureListWidget* listWidget = qobject_cast<const FeatureListWidget*>(opt.widget);
        const bool isDragSource = listWidget && listWidget->dragSourceRow() == index.row();
        const bool selected = opt.state & QStyle::State_Selected;
        const QColor textColor = opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        const QColor mutedColor = featureListMutedTextColor(opt.palette);
        const QColor modeColor = mutedColor;

        painter->save();
        painter->setClipRect(opt.rect);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        if (isDragSource) {
            painter->fillRect(opt.rect, opt.palette.color(QPalette::Midlight));
        } else {
            paintFeatureListRowChrome(painter, opt.rect, selected, isRunning, opt.palette);
        }

        const Feature* feature =
            m_panel->projectFeatureById(featureId);
        const bool isRunningFeature = isRunning;
        const bool holdMode =
            feature && normalizeRunMode(feature->runMode()) == FeatureRunMode::Hold;
        const bool hasBlocks = feature && !feature->workflow().blocks().empty();
        const bool runEnabled = isRunningFeature || (hasBlocks && !holdMode);
        paintFeatureRunButton(painter,
                              cols.run,
                              isRunningFeature,
                              runEnabled,
                              opt.palette);

        paintFeatureName(painter,
                         cols.name,
                         opt,
                         featureName,
                         isRunning && !featureName.isEmpty() && !isDragSource,
                         m_panel->animationPhase());

        QFont secondaryFont = opt.font;
        secondaryFont.setPointSize(qMax(secondaryFont.pointSize() - 1, 8));
        const QColor dragMuted = featureListMutedTextColor(opt.palette);
        drawCellText(painter,
                     cols.mode,
                     modeText,
                     secondaryFont,
                     isDragSource ? dragMuted : modeColor);
        if (hasHotkey) {
            const qreal iconOpacity = isDragSource ? 0.55 : 1.0;
            drawHotkeyBindingInRect(painter, cols.hotkey, hotkeyBinding, iconOpacity);
        } else {
            drawCellText(painter,
                         cols.hotkey,
                         QStringLiteral("\u2014"),
                         secondaryFont,
                         isDragSource ? dragMuted : mutedColor);
        }
        painter->restore();
    }
private:
    const FeatureListPanel* m_panel = nullptr;
};
int readLayoutInt(const QSettings& settings, const QString& key, int fallback) {
    if (!settings.contains(key)) {
        return fallback;
    }
    return qBound(0, settings.value(key).toInt(), 1000);
}
} // namespace
FeatureListPanel::FeatureListPanel(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}
void FeatureListPanel::setupUi() {
    setMinimumWidth(160);
    applyFeatureListPanelStyle(this);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* group = new QGroupBox(tr("기능 목록"), this);
    group->setObjectName(QStringLiteral("featureListGroup"));
    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(6, 4, 6, 6);
    groupLayout->setSpacing(6);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(6);
    m_addButton = new QPushButton(tr("추가"), group);
    m_editButton = new QPushButton(tr("편집"), group);
    m_removeButton = new QPushButton(tr("삭제"), group);
    m_addButton->setProperty("class", "featureListToolButton");
    m_editButton->setProperty("class", "featureListToolButton");
    m_removeButton->setProperty("class", "featureListToolButton");
    m_addButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_editButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_removeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    buttonRow->addWidget(m_addButton);
    buttonRow->addWidget(m_editButton);
    buttonRow->addWidget(m_removeButton);

    auto* tableFrame = new QFrame(group);
    tableFrame->setObjectName(QStringLiteral("featureListTableFrame"));
    tableFrame->setFrameShape(QFrame::NoFrame);
    auto* tableLayout = new QVBoxLayout(tableFrame);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(0);

    m_headerRow = new FeatureListHeaderWidget(this);
    m_headerRow->setToolTip(
        tr("헤더 구분선을 드래그해 열 너비·줄 높이를 조절합니다.\n"
           "· 기능|방식 경계: 방식 열 너비\n"
           "· 방식|키 경계: 키 열 너비\n"
           "· 헤더 아래 가장자리: 줄 높이"));

    m_list = new FeatureListWidget(tableFrame);
    m_list->setObjectName(QStringLiteral("featureListView"));
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setUniformItemSizes(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setSpacing(0);
    m_list->setItemDelegate(new FeatureListItemDelegate(this));
    connect(m_list, &FeatureListWidget::featureRowsReordered, this, &FeatureListPanel::onFeatureRowsReordered);
    connect(m_list, &FeatureListWidget::deleteRequested, this, &FeatureListPanel::onRemoveFeature);
    m_list->viewport()->installEventFilter(this);

    tableLayout->addWidget(m_headerRow);
    tableLayout->addWidget(m_list, 1);

    groupLayout->addLayout(buttonRow);
    groupLayout->addWidget(tableFrame, 1);
    outerLayout->addWidget(group);

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(55);
    connect(m_animTimer, &QTimer::timeout, this, &FeatureListPanel::onAnimationTick);
    connect(m_addButton, &QPushButton::clicked, this, &FeatureListPanel::onAddFeature);
    connect(m_removeButton, &QPushButton::clicked, this, &FeatureListPanel::onRemoveFeature);
    connect(m_editButton, &QPushButton::clicked, this, &FeatureListPanel::onEditFeature);
    connect(m_list, &QListWidget::currentRowChanged, this, &FeatureListPanel::onSelectionChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onEditFeature();
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, &FeatureListPanel::onContextMenu);
    updateReorderEnabled();
}

void FeatureListPanel::setColumnLayout(const FeatureListColumnLayout& layout, bool persist) {
    FeatureListColumnLayout clamped = layout;
    clamped.modeColumnWidth =
        qBound(kMinModeColumnWidth, layout.modeColumnWidth, kMaxModeColumnWidth);
    clamped.hotkeyColumnWidth =
        qBound(kMinHotkeyColumnWidth, layout.hotkeyColumnWidth, kMaxHotkeyColumnWidth);
    clamped.rowHeight = qBound(kMinRowHeight, layout.rowHeight, kMaxRowHeight);
    if (clamped.modeColumnWidth == m_columnLayout.modeColumnWidth
        && clamped.hotkeyColumnWidth == m_columnLayout.hotkeyColumnWidth
        && clamped.rowHeight == m_columnLayout.rowHeight) {
        return;
    }
    m_columnLayout = clamped;
    applyColumnLayoutToList();
    if (m_headerRow) {
        static_cast<FeatureListHeaderWidget*>(m_headerRow)->syncToLayout();
    }
    if (persist && !m_restoringColumnLayout) {
        emit columnLayoutChanged();
    }
}
void FeatureListPanel::applyColumnLayoutToList() {
    if (!m_list) {
        return;
    }
    for (int row = 0; row < m_list->count(); ++row) {
        if (QListWidgetItem* item = m_list->item(row)) {
            item->setSizeHint(QSize(0, m_columnLayout.rowHeight));
        }
    }
    m_list->doItemsLayout();
    if (m_list->viewport()) {
        m_list->viewport()->update();
    }
}
void FeatureListPanel::saveColumnLayout(QSettings& settings, const QString& settingsKey) const {
    settings.setValue(settingsKey + QStringLiteral("/modeColumnWidth"), m_columnLayout.modeColumnWidth);
    settings.setValue(settingsKey + QStringLiteral("/hotkeyColumnWidth"), m_columnLayout.hotkeyColumnWidth);
    settings.setValue(settingsKey + QStringLiteral("/rowHeight"), m_columnLayout.rowHeight);
}
void FeatureListPanel::restoreColumnLayout(const QSettings& settings, const QString& settingsKey) {
    if (!settings.contains(settingsKey + QStringLiteral("/modeColumnWidth"))
        && !settings.contains(settingsKey + QStringLiteral("/rowHeight"))
        && !settings.contains(settingsKey + QStringLiteral("/columnGap"))) {
        return;
    }
    FeatureListColumnLayout layout = m_columnLayout;
    layout.modeColumnWidth = qBound(kMinModeColumnWidth,
                                    readLayoutInt(settings,
                                                  settingsKey + QStringLiteral("/modeColumnWidth"),
                                                  layout.modeColumnWidth),
                                    kMaxModeColumnWidth);
    layout.hotkeyColumnWidth = qBound(kMinHotkeyColumnWidth,
                                      readLayoutInt(settings,
                                                    settingsKey + QStringLiteral("/hotkeyColumnWidth"),
                                                    layout.hotkeyColumnWidth),
                                      kMaxHotkeyColumnWidth);
    layout.rowHeight = qBound(kMinRowHeight,
                              readLayoutInt(settings,
                                            settingsKey + QStringLiteral("/rowHeight"),
                                            layout.rowHeight),
                              kMaxRowHeight);
    m_restoringColumnLayout = true;
    setColumnLayout(layout, false);
    m_restoringColumnLayout = false;
}
void FeatureListPanel::setRunningFeatureIds(const QSet<QString>& featureIds) {
    m_runningFeatureIds = featureIds;
    if (m_runningFeatureIds.isEmpty()) {
        m_animPhase = 0;
        if (m_animTimer) {
            m_animTimer->stop();
        }
    } else if (m_animTimer && !m_animTimer->isActive()) {
        m_animTimer->start();
    }
    updateReorderEnabled();
    if (m_list && m_list->viewport()) {
        m_list->viewport()->update();
    }
}
QString FeatureListPanel::runningFeatureId() const {
    if (m_runningFeatureIds.isEmpty()) {
        return {};
    }
    return *m_runningFeatureIds.constBegin();
}
bool FeatureListPanel::isFeatureRunning(const QString& featureId) const {
    return m_runningFeatureIds.contains(featureId);
}
void FeatureListPanel::setEditControlsEnabled(bool enabled) {
    m_editControlsEnabled = enabled;
    if (m_editButton) {
        m_editButton->setEnabled(enabled);
    }
    if (m_removeButton) {
        m_removeButton->setEnabled(enabled);
    }
    updateReorderEnabled();
}

void FeatureListPanel::updateReorderEnabled() {
    if (!m_list) {
        return;
    }
    m_list->setReorderEnabled(m_editControlsEnabled && m_runningFeatureIds.isEmpty());
}

const Feature* FeatureListPanel::projectFeatureById(const QString& featureId) const {
    if (!m_project || featureId.isEmpty()) {
        return nullptr;
    }
    return m_project->featureById(featureId.toStdString());
}

bool FeatureListPanel::runButtonHitTest(int row, const QPoint& viewportPos) const {
    if (!m_list || row < 0 || row >= m_list->count()) {
        return false;
    }
    QListWidgetItem* item = m_list->item(row);
    if (!item) {
        return false;
    }
    const QRect itemRect = m_list->visualItemRect(item);
    const FeatureListColumnRects cols = featureListColumnRects(itemRect, m_columnLayout);
    return featureRunButtonHitRect(cols.run).contains(viewportPos);
}

void FeatureListPanel::requestFeatureRun(int row) {
    if (!m_list || !m_project || row < 0 || row >= m_list->count()) {
        return;
    }
    QListWidgetItem* item = m_list->item(row);
    if (!item) {
        return;
    }
    const QString featureId = item->data(kFeatureIdRole).toString();
    const Feature* feature = projectFeatureById(featureId);
    if (!feature) {
        return;
    }
    const bool running = isFeatureRunning(featureId);
    const bool holdMode = normalizeRunMode(feature->runMode()) == FeatureRunMode::Hold;
    const bool hasBlocks = !feature->workflow().blocks().empty();
    if (!running && (holdMode || !hasBlocks)) {
        return;
    }
    emit featureRunRequested(featureId);
}

bool FeatureListPanel::eventFilter(QObject* watched, QEvent* event) {
    if (m_list && watched == m_list->viewport() && event->type() == QEvent::MouseButtonRelease) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QListWidgetItem* item = m_list->itemAt(mouseEvent->pos());
            if (item) {
                const int row = m_list->row(item);
                if (runButtonHitTest(row, mouseEvent->pos())) {
                    requestFeatureRun(row);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FeatureListPanel::onFeatureRowsReordered(int fromRow, int toRow) {
    if (!m_project) {
        if (m_list) {
            refresh();
        }
        return;
    }
    if (fromRow != toRow) {
        m_project->moveFeature(fromRow, toRow);
        emit projectModified();
    }
    refresh();
    if (fromRow != toRow) {
        m_list->setCurrentRow(toRow);
    } else if (fromRow >= 0 && fromRow < m_list->count()) {
        m_list->setCurrentRow(fromRow);
    }
}
void FeatureListPanel::onAnimationTick() {
    m_animPhase = (m_animPhase + 1) % 72;
    if (m_list && m_list->viewport()) {
        m_list->viewport()->update();
    }
}
void FeatureListPanel::setProject(Project* project) {
    m_project = project;
    refresh();
}
void FeatureListPanel::configureListItem(QListWidgetItem* item, const Feature& feature) {
    const QString featureName = QString::fromStdString(feature.name());
    const QString modeText = featureRunModeCompact(feature.runMode(), feature.repeatCount());
    const HotkeyBinding& hotkey = feature.hotkey();
    item->setText(featureName);
    item->setSizeHint(QSize(0, m_columnLayout.rowHeight));
    item->setData(kFeatureIdRole, QString::fromStdString(feature.id()));
    item->setData(kFeatureNameRole, featureName);
    item->setData(kRunModeDisplayRole, modeText);
    item->setData(kHotkeyVirtualKeyRole, hotkey.virtualKey);
    item->setData(kHotkeyCtrlRole, hotkey.ctrl);
    item->setData(kHotkeyAltRole, hotkey.alt);
    item->setData(kHotkeyShiftRole, hotkey.shift);
    QString tooltip = featureName;
    tooltip += QStringLiteral("\n") + tr("동작: %1").arg(featureRunModeLabel(feature.runMode()));
    if (normalizeRunMode(feature.runMode()) == FeatureRunMode::RepeatCount) {
        tooltip += tr(" (%1회)").arg(feature.repeatCount());
    }
    if (feature.infiniteExitAfterConsecutiveMisses() > 0) {
        tooltip += QStringLiteral("\n")
                   + tr("연속 감지 실패 %1회 시 종료").arg(feature.infiniteExitAfterConsecutiveMisses());
    }
    if (!feature.hotkey().isEmpty()) {
        tooltip += QStringLiteral("\n") + tr("단축키: %1").arg(feature.hotkey().displayString());
    } else {
        tooltip += QStringLiteral("\n") + tr("단축키 없음");
    }
    item->setToolTip(tooltip);
}
void FeatureListPanel::refresh() {
    const int previousRow = m_list->currentRow();
    if (m_lastSelectedFeatureId.isEmpty() && previousRow >= 0) {
        if (Feature* feature = selectedFeature()) {
            m_lastSelectedFeatureId = QString::fromStdString(feature->id());
        }
    }
    m_list->clear();
    if (!m_project) {
        return;
    }
    int restoreRow = -1;
    for (int i = 0; i < static_cast<int>(m_project->features().size()); ++i) {
        auto* item = new QListWidgetItem(m_list);
        configureListItem(item, *m_project->features()[i]);
        if (restoreRow < 0 && !m_lastSelectedFeatureId.isEmpty()
            && item->data(kFeatureIdRole).toString() == m_lastSelectedFeatureId) {
            restoreRow = i;
        }
    }
    if (restoreRow >= 0) {
        m_list->setCurrentRow(restoreRow);
    } else if (previousRow >= 0 && previousRow < m_list->count()) {
        m_list->setCurrentRow(previousRow);
    }
    if (!m_runningFeatureIds.isEmpty() && m_list->viewport()) {
        m_list->viewport()->update();
    }
}
Feature* FeatureListPanel::selectedFeature() const {
    if (!m_project) {
        return nullptr;
    }
    return m_project->featureAt(selectedIndex());
}
int FeatureListPanel::selectedIndex() const {
    return m_list->currentRow();
}
QString FeatureListPanel::selectedFeatureId() const {
    if (!m_list || m_list->currentRow() < 0) {
        return m_lastSelectedFeatureId;
    }
    QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return m_lastSelectedFeatureId;
    }
    return item->data(kFeatureIdRole).toString();
}
void FeatureListPanel::selectFeatureById(const QString& featureId) {
    if (!m_list || featureId.isEmpty()) {
        return;
    }
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (!item || item->data(kFeatureIdRole).toString() != featureId) {
            continue;
        }
        m_lastSelectedFeatureId = featureId;
        m_list->setCurrentRow(row);
        return;
    }
}
void FeatureListPanel::onAddFeature() {
    if (!m_project) {
        return;
    }
    m_project->addFeature(tr("새 기능").toStdString());
    refresh();
    const int index = static_cast<int>(m_project->features().size()) - 1;
    m_list->setCurrentRow(index);
    editFeatureAt(index);
}
void FeatureListPanel::onRemoveFeature() {
    if (!m_project || !m_editControlsEnabled) {
        return;
    }
    const int index = selectedIndex();
    if (index < 0) {
        return;
    }
    QMessageBox box(QMessageBox::Question, tr("기능 삭제"), tr("선택한 기능을 삭제할까요?"),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.button(QMessageBox::Yes)->setText(tr("예"));
    box.button(QMessageBox::No)->setText(tr("아니오"));
    if (static_cast<QMessageBox::StandardButton>(box.exec()) != QMessageBox::Yes) {
        return;
    }
    m_project->removeFeature(index);
    refresh();
    emit selectionChanged();
    emit projectModified();
    emit hotkeysChanged();
}
bool FeatureListPanel::editFeatureAt(int index) {
    if (!m_editControlsEnabled) {
        return false;
    }
    if (!m_project || index < 0 || index >= static_cast<int>(m_project->features().size())) {
        return false;
    }
    Feature* feature = m_project->featureAt(index);
    if (!feature) {
        return false;
    }
    const HotkeyBinding previousHotkey = feature->hotkey();
    const FeatureRunMode previousMode = feature->runMode();
    FeatureEditDialog dialog(QString::fromStdString(feature->name()),
                             feature->hotkey(),
                             feature->runMode(),
                             feature->repeatCount(),
                             feature->infiniteExitAfterConsecutiveMisses(),
                             feature->userInputInterruptMode(),
                             m_project,
                             feature->id(),
                             this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    feature->setName(dialog.featureName().toStdString());
    feature->setHotkey(dialog.hotkey());
    feature->setRunMode(dialog.runMode());
    feature->setRepeatCount(dialog.repeatCount());
    feature->setInfiniteExitAfterConsecutiveMisses(dialog.infiniteExitAfterConsecutiveMisses());
    feature->setUserInputInterruptMode(dialog.userInputInterruptMode());
    refresh();
    m_list->setCurrentRow(index);
    emit projectModified();
    if (previousHotkey != feature->hotkey() || previousMode != feature->runMode()) {
        emit hotkeysChanged();
    }
    return true;
}
void FeatureListPanel::onEditFeature() {
    editFeatureAt(selectedIndex());
}

void FeatureListPanel::onContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_list->itemAt(pos);
    if (!item) {
        return;
    }
    const QString featureId = item->data(kFeatureIdRole).toString();
    Feature* feature =
        m_project ? m_project->featureById(featureId.toStdString()) : nullptr;
    QMenu menu(this);
    const bool running = feature && isFeatureRunning(featureId);
    const bool canRun = feature && !running
                        && normalizeRunMode(feature->runMode()) != FeatureRunMode::Hold
                        && !feature->workflow().blocks().empty();
    if (running) {
        menu.addAction(tr("중지"), this, [this, featureId]() {
            emit featureRunRequested(featureId);
        });
    } else if (canRun) {
        menu.addAction(tr("실행"), this, [this, featureId]() {
            emit featureRunRequested(featureId);
        });
    }
    menu.addAction(tr("편집"), this, &FeatureListPanel::onEditFeature);
    menu.addAction(tr("삭제"), this, &FeatureListPanel::onRemoveFeature);
    menu.exec(m_list->mapToGlobal(pos));
}
void FeatureListPanel::onSelectionChanged() {
    if (Feature* feature = selectedFeature()) {
        m_lastSelectedFeatureId = QString::fromStdString(feature->id());
    }
    emit selectionChanged();
}
