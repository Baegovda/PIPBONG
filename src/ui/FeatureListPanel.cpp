#include "ui/FeatureListPanel.h"
#include "ui/FeatureListWidget.h"
#include "ui/FeatureLibraryListWidget.h"
#include "ui/widgets/ReorderableListWidget.h"
#include "ui/widgets/ListColumnHeaderWidget.h"
#include "ui/UiResizeHandle.h"
#include "ui/HotkeyBindingIcon.h"
#include "model/Feature.h"
#include "model/FeatureRunMode.h"
#include "model/Project.h"
#include "core/input/HotkeyBinding.h"
#include "ui/editors/FeatureEditDialog.h"
#include "ui/editors/FeatureEditPrefs.h"
#include "ui/UiThemeColors.h"
#include "ui/UiHoverFeedback.h"
#include "app/FeatureHotkeyGate.h"
#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
#include <QLineEdit>
#include <QAbstractItemView>
#include <climits>
#include <cmath>
namespace {
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
constexpr int kMinEnableColumnWidth = 22;
constexpr int kMaxEnableColumnWidth = 48;
constexpr int kMinRunColumnWidth = 20;
constexpr int kMaxRunColumnWidth = 40;

// Refined jade-teal for trigger mode — distinct from normal run blue and active-run prism.
constexpr QColor kTriggerWatchWash(132, 186, 172);
constexpr QColor kTriggerCooldownWash(138, 144, 152);
constexpr QColor kTriggerWatchAccent(78, 168, 148);
constexpr QColor kTriggerCooldownAccent(118, 126, 136);
struct FeatureListColumnRects {
    QRect enable;
    QRect run;
    QRect name;
    QRect mode;
    QRect hotkey;
};
struct FeatureListColumnEdges {
    FeatureListColumnRects cols;
    int runDividerX = 0;
    int nameDividerX = 0;
    int modeDividerX = 0;
    int hotkeyDividerX = 0;
};
FeatureListColumnEdges featureListColumnEdges(const QRect& itemRect, const FeatureListColumnLayout& layout) {
    FeatureListColumnEdges edges;
    const QRect content = itemRect.adjusted(kColumnMargin, 0, -kColumnMargin, 0);

    const int enableLeft = content.left();
    const int runLeft = enableLeft + layout.enableColumnWidth + kColumnGap;
    const int nameLeft = runLeft + layout.runColumnWidth + kColumnGap;
    const int hotkeyLeft = content.right() - layout.hotkeyColumnWidth + 1;
    const int modeLeft = hotkeyLeft - kColumnGap - layout.modeColumnWidth;
    const int nameWidth = qMax(0, modeLeft - kColumnGap - nameLeft);

    edges.runDividerX = runLeft;
    edges.nameDividerX = nameLeft;
    edges.hotkeyDividerX = hotkeyLeft;
    edges.modeDividerX = modeLeft;
    edges.cols.enable = QRect(enableLeft, content.top(), layout.enableColumnWidth, content.height());
    edges.cols.run = QRect(runLeft, content.top(), layout.runColumnWidth, content.height());
    edges.cols.hotkey = QRect(hotkeyLeft, content.top(), layout.hotkeyColumnWidth, content.height());
    edges.cols.mode = QRect(modeLeft, content.top(), layout.modeColumnWidth, content.height());
    edges.cols.name = QRect(nameLeft, content.top(), nameWidth, content.height());
    return edges;
}

enum class FeatureListResizeHandleId : int {
    None = 0,
    Run = 1,
    RunName = 2,
    Mode = 3,
    Hotkey = 4,
};

int featureListDividerHandleAt(const QPoint& pos, const FeatureListColumnEdges& edges) {
    struct Candidate {
        int x = 0;
        FeatureListResizeHandleId handle = FeatureListResizeHandleId::None;
    };
    const QList<Candidate> candidates = {{edges.runDividerX, FeatureListResizeHandleId::Run},
                                         {edges.nameDividerX, FeatureListResizeHandleId::RunName},
                                         {edges.modeDividerX, FeatureListResizeHandleId::Mode},
                                         {edges.hotkeyDividerX, FeatureListResizeHandleId::Hotkey}};

    FeatureListResizeHandleId best = FeatureListResizeHandleId::None;
    int bestDistance = INT_MAX;
    for (const Candidate& candidate : candidates) {
        if (candidate.x <= 0
            || !UiResizeHandle::isWithinHorizontalGrab(pos.x(), candidate.x)) {
            continue;
        }
        const int distance = qAbs(pos.x() - candidate.x);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate.handle;
        }
    }
    return static_cast<int>(best);
}

QList<int> featureListDividerXs(const FeatureListColumnEdges& edges) {
    return {edges.runDividerX,
            edges.nameDividerX,
            edges.modeDividerX,
            edges.hotkeyDividerX};
}
FeatureListColumnRects featureListColumnRects(const QRect& itemRect, const FeatureListColumnLayout& layout) {
    return featureListColumnEdges(itemRect, layout).cols;
}
QString featureRunModeLabel(FeatureRunMode mode) {
    switch (mode) {
    case FeatureRunMode::Hold:
        return QObject::tr("홀드");
    case FeatureRunMode::RepeatInfinite:
        return QObject::tr("무한");
    case FeatureRunMode::Trigger:
        return QObject::tr("트리거");
    case FeatureRunMode::RepeatCount:
        return QObject::tr("N회");
    }
    return QObject::tr("N회");
}
QString featureRunModeCompact(FeatureRunMode mode, int repeatCount) {
    switch (mode) {
    case FeatureRunMode::Hold:
        return QObject::tr("홀드");
    case FeatureRunMode::RepeatInfinite:
        return QStringLiteral("\u221e");
    case FeatureRunMode::Trigger:
        return QObject::tr("T");
    case FeatureRunMode::RepeatCount:
        return repeatCount <= 1 ? QStringLiteral("\u00d71") : QStringLiteral("\u00d7%1").arg(repeatCount);
    }
    return QStringLiteral("\u00d71");
}

QString triggerCooldownModeDisplayText(qint64 remainingMs) {
    const int remainSec = static_cast<int>((qMax<qint64>(0, remainingMs) + 999) / 1000);
    return QObject::tr("%1초").arg(remainSec);
}

QRect featureRunButtonHitRect(const QRect& runColumnRect);

void paintTriggerCooldownRunButton(QPainter* painter,
                                   const QRect& runColumnRect,
                                   qint64 remainingMs,
                                   int animationPhase,
                                   const QColor& accent,
                                   const QPalette& palette) {
    const QRect btnRect = featureRunButtonHitRect(runColumnRect);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor track = palette.color(QPalette::Mid);
    track.setAlpha(72);
    painter->setPen(Qt::NoPen);
    painter->setBrush(track);
    painter->drawEllipse(btnRect);

    QColor spinColor = accent.isValid() ? accent : palette.color(QPalette::Highlight);
    QPen arcPen(spinColor, qMax(2.0, btnRect.width() * 0.13));
    arcPen.setCapStyle(Qt::RoundCap);
    painter->setPen(arcPen);
    painter->setBrush(Qt::NoBrush);
    const QRect arcRect = btnRect.adjusted(2, 2, -2, -2);
    const int startAngle = static_cast<int>(animationPhase * 15 * 16);
    const int spanAngle = 270 * 16;
    painter->drawArc(arcRect, startAngle, spanAngle);

    const int remainSec = static_cast<int>((qMax<qint64>(0, remainingMs) + 999) / 1000);
    QFont font = painter->font();
    font.setPointSize(qMax(7, btnRect.height() / 3));
    font.setBold(true);
    painter->setFont(font);
    QColor textColor = palette.color(QPalette::WindowText);
    if (accent.isValid()) {
        textColor = spinColor;
    }
    painter->setPen(textColor);
    painter->drawText(btnRect, Qt::AlignCenter, QString::number(remainSec));

    painter->restore();
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

QRect featureEnableToggleHitRect(const QRect& enableColumnRect) {
    const int size = qBound(12, qMin(enableColumnRect.width(), enableColumnRect.height()) - 8, 16);
    return QRect(enableColumnRect.left() + (enableColumnRect.width() - size) / 2,
                 enableColumnRect.top() + (enableColumnRect.height() - size) / 2,
                 size,
                 size);
}

void paintFeatureEnableToggle(QPainter* painter,
                              const QRect& enableColumnRect,
                              bool enabled,
                              const QPalette& palette,
                              const QColor* runningAccent = nullptr) {
    const QRect box = featureEnableToggleHitRect(enableColumnRect);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QColor content = primaryContentTextColor(palette, false);
    QColor accent = content.lightness() < 128 ? QColor(0x4a, 0x90, 0xd9) : QColor(0x1e, 0x88, 0xe5);
    if (runningAccent && runningAccent->isValid()) {
        accent = *runningAccent;
    }
    const QColor muted = content.lightness() < 128 ? QColor(0x8a, 0x96, 0xa8) : QColor(0x90, 0x9a, 0xaa);

    if (enabled && runningAccent && runningAccent->isValid()) {
        QColor ring = *runningAccent;
        ring.setAlpha(72);
        painter->setPen(QPen(ring, 1.2));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(box.adjusted(-2, -2, 2, 2));
    }

    if (enabled) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(accent);
        painter->drawEllipse(box);

        QPainterPath check;
        const qreal x0 = box.left() + box.width() * 0.24;
        const qreal y0 = box.center().y() + box.height() * 0.02;
        const qreal x1 = box.left() + box.width() * 0.42;
        const qreal y1 = box.bottom() - box.height() * 0.30;
        const qreal x2 = box.right() - box.width() * 0.22;
        const qreal y2 = box.top() + box.height() * 0.28;
        check.moveTo(x0, y0);
        check.lineTo(x1, y1);
        check.lineTo(x2, y2);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::white, qMax(1.6, box.width() * 0.14), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(check);
    } else {
        painter->setPen(QPen(muted, 1.35));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QRectF(box).adjusted(0.6, 0.6, -0.6, -0.6));
    }

    painter->restore();
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
                           const QPalette& palette,
                           const QColor* runningAccent = nullptr) {
    const QRect btnRect = featureRunButtonHitRect(runColumnRect);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor fill = enabled ? palette.color(QPalette::Highlight) : palette.color(QPalette::Mid);
    if (!enabled) {
        fill.setAlpha(120);
    } else if (showStop && runningAccent && runningAccent->isValid()) {
        fill = *runningAccent;
        fill.setAlpha(static_cast<int>(qBound(150, fill.alpha() > 0 ? fill.alpha() : 220, 255)));
    } else if (showStop) {
        fill = fill.darker(108);
    }

    if (showStop && runningAccent && runningAccent->isValid()) {
        QColor glow = *runningAccent;
        glow.setAlpha(48);
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawRoundedRect(btnRect.adjusted(-2, -2, 2, 2), 6, 6);
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

struct PrismRunningTone {
    QColor core;
    QColor coolGlow;
    QColor warmGlow;
    qreal shimmer = 1.0;
};

PrismRunningTone prismRunningTone(int animationPhase, bool selected) {
    const qreal t = static_cast<qreal>(animationPhase % 96) / 96.0;
    const qreal shimmer = 0.62 + 0.38 * std::sin(t * 2.0 * M_PI);

    // Curated prism hues — lavender → azure → ice → lilac → rose (no harsh full-spectrum rainbow).
    static constexpr qreal kPrismHues[] = {252.0, 224.0, 196.0, 268.0, 296.0, 252.0};
    static constexpr int kPrismHueCount = 5;
    const qreal pos = std::fmod(t * static_cast<qreal>(kPrismHueCount), static_cast<qreal>(kPrismHueCount));
    const int index = static_cast<int>(std::floor(pos));
    const qreal frac = pos - static_cast<qreal>(index);

    auto lerpHue = [](qreal from, qreal to, qreal amount) {
        qreal delta = to - from;
        if (delta > 180.0) {
            delta -= 360.0;
        } else if (delta < -180.0) {
            delta += 360.0;
        }
        qreal hue = from + delta * amount;
        while (hue < 0.0) {
            hue += 360.0;
        }
        while (hue >= 360.0) {
            hue -= 360.0;
        }
        return hue;
    };

    const qreal hue =
        lerpHue(kPrismHues[index], kPrismHues[index + 1], frac);
    const int saturation = selected ? 108 : 128;
    const int value = selected ? 250 : 255;

    PrismRunningTone tone;
    tone.core = QColor::fromHsv(static_cast<int>(hue), saturation, value);
    tone.coolGlow = QColor::fromHsv(static_cast<int>(hue + 14.0) % 360, 88, 255);
    tone.warmGlow = QColor::fromHsv(static_cast<int>(hue + 346.0) % 360, 84, 255);
    tone.shimmer = shimmer;
    return tone;
}

void paintPrismActiveRunRowChrome(QPainter* painter,
                                  const QRect& rect,
                                  bool selected,
                                  bool hovered,
                                  int animationPhase,
                                  const QPalette& palette) {
    const PrismRunningTone prism = prismRunningTone(animationPhase, selected);
    const PrismRunningTone prismShift = prismRunningTone((animationPhase + 28) % 96, selected);
    const qreal t = static_cast<qreal>(animationPhase % 96) / 96.0;

    const QRect card = rect.adjusted(2, 1, -2, -kRowSeparatorHeight);
    QPainterPath cardPath;
    cardPath.addRoundedRect(card, 6, 6);

    QColor baseBg = palette.color(QPalette::Base);
    if (selected) {
        const QColor highlight = palette.color(QPalette::Highlight);
        baseBg = QColor(baseBg.red() + (highlight.red() - baseBg.red()) / 5,
                        baseBg.green() + (highlight.green() - baseBg.green()) / 5,
                        baseBg.blue() + (highlight.blue() - baseBg.blue()) / 5);
    } else if (hovered) {
        baseBg = UiHoverFeedback::blendListRowHover(palette);
    }
    baseBg = QColor((baseBg.red() * 5 + prism.core.red()) / 6,
                    (baseBg.green() * 5 + prism.core.green()) / 6,
                    (baseBg.blue() * 5 + prism.core.blue()) / 6);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setClipRect(rect);

    for (int ring = 2; ring >= 0; --ring) {
        const int spread = 1 + ring;
        QColor halo = prism.coolGlow;
        halo.setAlpha(static_cast<int>((18 - ring * 5) * prism.shimmer));
        painter->setPen(Qt::NoPen);
        painter->setBrush(halo);
        painter->drawRoundedRect(card.adjusted(-spread, -spread, spread, spread), 6 + spread, 6 + spread);
    }

    painter->fillPath(cardPath, baseBg);

    QLinearGradient wash(card.topLeft(), card.topRight());
    QColor washLeft = prism.coolGlow;
    washLeft.setAlpha(static_cast<int>(34 * prism.shimmer));
    QColor washMid = prism.core;
    washMid.setAlpha(static_cast<int>(46 * prism.shimmer));
    QColor washRight = prismShift.warmGlow;
    washRight.setAlpha(static_cast<int>(30 * prism.shimmer));
    wash.setColorAt(0.0, washLeft);
    wash.setColorAt(0.42, washMid);
    wash.setColorAt(1.0, washRight);
    painter->fillPath(cardPath, wash);

    QRadialGradient radial(card.center(), qMax(card.width(), card.height()) * 0.58);
    QColor bloom = prism.core;
    bloom.setAlpha(static_cast<int>(26 * prism.shimmer));
    radial.setColorAt(0.0, bloom);
    bloom.setAlpha(0);
    radial.setColorAt(1.0, bloom);
    painter->fillPath(cardPath, radial);

    const qreal sheenT = std::fmod(t * 1.18 + 0.06, 1.0);
    const int sheenCenterX = card.left() + static_cast<int>(card.width() * sheenT);
    QLinearGradient sheen(sheenCenterX - 52, card.top(), sheenCenterX + 52, card.top());
    sheen.setColorAt(0.0, QColor(255, 255, 255, 0));
    sheen.setColorAt(0.5, QColor(255, 255, 255, static_cast<int>(20 * prism.shimmer)));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter->fillPath(cardPath, sheen);

    QLinearGradient topGlass(card.topLeft(), QPoint(card.left(), card.top() + card.height() / 2));
    topGlass.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(14 * prism.shimmer)));
    topGlass.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter->fillPath(cardPath, topGlass);

    QColor borderColor = prism.core;
    borderColor.setAlpha(static_cast<int>(88 + 72 * prism.shimmer));
    painter->setPen(QPen(borderColor, 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(cardPath);

    const QRect accentBar(card.left(), card.top(), kSelectionBarWidth, card.height());
    QLinearGradient accentGrad(accentBar.topLeft(), accentBar.bottomLeft());
    QColor accentTop = prism.coolGlow;
    accentTop.setAlpha(255);
    QColor accentBottom = prism.warmGlow;
    accentBottom.setAlpha(255);
    accentGrad.setColorAt(0.0, accentTop);
    accentGrad.setColorAt(1.0, accentBottom);
    painter->fillRect(accentBar, accentGrad);

    if (selected) {
        painter->fillRect(QRect(rect.left(), rect.top(), kSelectionBarWidth, rect.height()),
                          palette.color(QPalette::Highlight));
    }

    painter->restore();
}

void paintPrismRunningFeatureName(QPainter* painter,
                                  const QRect& rect,
                                  const QStyleOptionViewItem& option,
                                  const QFont& nameFont,
                                  const QString& elided,
                                  int animationPhase) {
    const Qt::Alignment align = Qt::AlignHCenter | Qt::AlignVCenter;
    const bool selected = option.state & QStyle::State_Selected;
    const PrismRunningTone prism = prismRunningTone(animationPhase, selected);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setFont(nameFont);

    QColor shadow = prism.core;
    shadow.setAlpha(static_cast<int>(42 * prism.shimmer));
    painter->setPen(shadow);
    painter->drawText(rect.adjusted(0, 1, 0, 1), align, elided);

    QColor foreground(236, 244, 252);
    if (selected) {
        foreground = QColor(255, 255, 255);
    }
    painter->setPen(foreground);
    painter->drawText(rect, align, elided);
    painter->restore();
}

void paintFeatureListRowChrome(QPainter* painter,
                               const QRect& rect,
                               bool selected,
                               bool running,
                               bool hovered,
                               FeatureRunVisualKind visualKind,
                               int animationPhase,
                               const QPalette& palette) {
    if (running && visualKind == FeatureRunVisualKind::ActiveRun) {
        paintPrismActiveRunRowChrome(painter, rect, selected, hovered, animationPhase, palette);
        QColor separator = palette.color(QPalette::Mid);
        if (darkThemeFromPalette(palette) && separator.lightness() < 90) {
            separator = separator.lighter(130);
        }
        separator.setAlpha(80);
        painter->fillRect(QRect(rect.left(), rect.bottom(), rect.width(), kRowSeparatorHeight), separator);
        return;
    }

    QColor rowBg = palette.color(QPalette::Base);
    if (selected) {
        const QColor highlight = palette.color(QPalette::Highlight);
        rowBg = QColor(rowBg.red() + (highlight.red() - rowBg.red()) / 6,
                       rowBg.green() + (highlight.green() - rowBg.green()) / 6,
                       rowBg.blue() + (highlight.blue() - rowBg.blue()) / 6);
    } else if (hovered) {
        rowBg = UiHoverFeedback::blendListRowHover(palette);
    }
    painter->fillRect(rect, rowBg);

    if (running && visualKind != FeatureRunVisualKind::ActiveRun) {
        const bool watch = visualKind == FeatureRunVisualKind::TriggerWatch;
        const qreal pulse = 0.5 + 0.5 * std::sin(animationPhase * M_PI / (watch ? 36.0 : 24.0));
        const QColor washBase = watch ? kTriggerWatchWash : kTriggerCooldownWash;
        const int washAlpha = watch ? static_cast<int>(12 + pulse * 14) : static_cast<int>(8 + pulse * 8);
        QColor wash = washBase;
        wash.setAlpha(washAlpha);
        const QRect inner = rect.adjusted(1, 2, -1, -2);
        QLinearGradient gradient(inner.topLeft(), inner.bottomRight());
        gradient.setColorAt(0.0, wash);
        QColor washFade = wash;
        washFade.setAlpha(washAlpha / 3);
        gradient.setColorAt(0.65, washFade);
        gradient.setColorAt(1.0, Qt::transparent);
        painter->fillRect(inner, gradient);
        QColor edge(228, 242, 238, watch ? static_cast<int>(14 + pulse * 10) : 10);
        painter->fillRect(QRect(inner.left(), inner.top(), inner.width(), 1), edge);
    }

    if (selected) {
        painter->fillRect(QRect(rect.left(), rect.top(), kSelectionBarWidth, rect.height()),
                          palette.color(QPalette::Highlight));
    } else if (running) {
        QColor accent = palette.color(QPalette::Highlight);
        if (visualKind == FeatureRunVisualKind::TriggerWatch) {
            const qreal pulse = 0.72 + 0.28 * std::sin(animationPhase * M_PI / 36.0);
            accent = kTriggerWatchAccent;
            accent.setAlpha(static_cast<int>(pulse * 190.0));
        } else if (visualKind == FeatureRunVisualKind::TriggerCooldown) {
            const qreal pulse = 0.55 + 0.25 * std::sin(animationPhase * M_PI / 24.0);
            accent = kTriggerCooldownAccent;
            accent.setAlpha(static_cast<int>(pulse * 150.0));
        } else if (visualKind == FeatureRunVisualKind::ActiveRun) {
            const PrismRunningTone prism = prismRunningTone(animationPhase, false);
            accent = prism.core;
            accent.setAlpha(static_cast<int>(prism.shimmer * 215.0));
        }
        painter->fillRect(QRect(rect.left(), rect.top(), kSelectionBarWidth, rect.height()), accent);
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
                      FeatureRunVisualKind visualKind,
                      int animationPhase) {
    QFont nameFont = option.font;
    if (runningGlow) {
        nameFont.setBold(true);
    }
    const QFontMetrics metrics(nameFont);
    const QString elided = elideCellText(metrics, name, rect.width());
    const Qt::Alignment align = Qt::AlignHCenter | Qt::AlignVCenter;
    const bool selected = option.state & QStyle::State_Selected;
    const QColor baseText = primaryContentTextColor(option.palette, selected);
    if (!runningGlow) {
        painter->setFont(nameFont);
        painter->setPen(baseText);
        painter->drawText(rect, align, elided);
        return;
    }

    if (visualKind == FeatureRunVisualKind::TriggerWatch) {
        const qreal pulse = 0.88 + 0.12 * std::sin(animationPhase * M_PI / 36.0);
        QColor nameColor = baseText;
        if (selected) {
            nameColor = option.palette.color(QPalette::HighlightedText);
        }
        QColor glow = kTriggerWatchWash;
        glow.setAlpha(static_cast<int>(24 * pulse));
        painter->setFont(nameFont);
        painter->setPen(glow);
        painter->drawText(rect.adjusted(-1, 0, 1, 0), align, elided);
        painter->setPen(nameColor);
        painter->drawText(rect, align, elided);
        return;
    }

    if (visualKind == FeatureRunVisualKind::TriggerCooldown) {
        const qreal pulse = 0.82 + 0.18 * std::sin(animationPhase * M_PI / 24.0);
        QColor nameColor = baseText;
        if (selected) {
            nameColor = option.palette.color(QPalette::HighlightedText);
        }
        nameColor.setAlpha(static_cast<int>(pulse * 255.0));
        painter->setFont(nameFont);
        painter->setPen(nameColor);
        painter->drawText(rect, align, elided);
        return;
    }

    paintPrismRunningFeatureName(painter, rect, option, nameFont, elided, animationPhase);
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
        "}"
        "QToolButton#featureLibraryToggle {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  padding: 5px 8px;"
        "  text-align: left;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "  background-color: palette(button);"
        "  color: palette(window-text);"
        "}"
        "QToolButton#featureLibraryToggle:hover {"
        "  background-color: palette(light);"
        "}"
        "QToolButton#featureLibraryToggle:checked {"
        "  background-color: palette(base);"
        "}"
        "QWidget#featureLibraryDrawerHost {"
        "  background-color: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "}"
        "QListWidget#featureLibraryList {"
        "  border: none;"
        "  background: transparent;"
        "  outline: none;"
        "}"
        "QListWidget#featureLibraryList::item {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0;"
        "}"
        "QListWidget#featureLibraryList::item:selected {"
        "  background: transparent;"
        "}"));
}
class FeatureListItemDelegate : public QStyledItemDelegate {
public:
    explicit FeatureListItemDelegate(const FeatureListPanel* panel)
        : m_panel(panel) {}
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        const int rowHeight = m_panel ? m_panel->columnLayout().rowHeight : 26;
        return {option.rect.width(), rowHeight};
    }
    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        Q_UNUSED(index);
        if (!m_panel) {
            return nullptr;
        }
        auto* edit = new QLineEdit(parent);
        edit->setFrame(true);
        edit->setFont(option.font);
        edit->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        return edit;
    }
    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override {
        Q_UNUSED(index);
        if (!m_panel || !editor) {
            return;
        }
        const FeatureListColumnRects cols = featureListColumnRects(option.rect, m_panel->columnLayout());
        editor->setGeometry(cols.name.adjusted(1, 1, -1, -1));
    }
    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto* line = qobject_cast<QLineEdit*>(editor);
        if (!line) {
            return;
        }
        line->setText(index.data(kFeatureNameRole).toString());
        line->selectAll();
    }
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        auto* line = qobject_cast<QLineEdit*>(editor);
        if (!line || !model) {
            return;
        }
        const QString name = line->text().trimmed();
        if (name.isEmpty()) {
            return;
        }
        model->setData(index, name, kFeatureNameRole);
        model->setData(index, name, Qt::DisplayRole);
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
        QString modeText = index.data(kRunModeDisplayRole).toString();
        const HotkeyBinding hotkeyBinding = hotkeyBindingFromIndex(index);
        const bool hasHotkey = !hotkeyBinding.isEmpty();
        const bool isRunning = m_panel->isFeatureRunning(featureId);
        const Feature* feature =
            m_panel->projectFeatureById(featureId);
        const bool featureEnabled = feature ? feature->enabled() : true;
        const FeatureListWidget* listWidget = qobject_cast<const FeatureListWidget*>(opt.widget);
        const bool isDragSource = listWidget && listWidget->dragSourceRow() == index.row();
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hovered = (opt.state & QStyle::State_MouseOver) && !selected;
        const QColor mutedColor = featureListMutedTextColor(opt.palette);

        painter->save();
        painter->setClipRect(opt.rect);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        if (isDragSource) {
            QColor fill = opt.palette.color(QPalette::Base);
            fill.setAlpha(72);
            painter->fillRect(opt.rect, fill);
            QColor border = opt.palette.color(QPalette::Mid);
            border.setAlpha(150);
            QPen pen(border, 1.5, Qt::DashLine);
            pen.setDashPattern({4, 3});
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(opt.rect.adjusted(2, 1, -2, -1), 5, 5);
            painter->restore();
            return;
        }

        paintFeatureListRowChrome(painter,
                                  opt.rect,
                                  selected,
                                  isRunning,
                                  hovered,
                                  m_panel->featureRunVisualKind(featureId),
                                  m_panel->animationPhase(),
                                  opt.palette);
        if (!featureEnabled) {
            QColor disabledOverlay = opt.palette.color(QPalette::Mid);
            disabledOverlay.setAlpha(48);
            painter->fillRect(opt.rect, disabledOverlay);
        }

        const FeatureRunVisualKind visualKind = m_panel->featureRunVisualKind(featureId);
        const int animationPhase = m_panel->animationPhase();
        if (visualKind == FeatureRunVisualKind::TriggerCooldown) {
            const qint64 remainingMs = m_panel->triggerCooldownRemainingMs(featureId);
            if (remainingMs >= 0) {
                modeText = triggerCooldownModeDisplayText(remainingMs);
            }
        } else if (visualKind == FeatureRunVisualKind::TriggerWatch) {
            modeText = QStringLiteral("\u25CE");
        }
        const bool prismRow =
            featureEnabled && isRunning && visualKind == FeatureRunVisualKind::ActiveRun;
        const PrismRunningTone prismTone =
            prismRow ? prismRunningTone(animationPhase, selected) : PrismRunningTone{};
        QColor triggerAccent;
        const QColor* runAccent = nullptr;
        if (prismRow) {
            runAccent = &prismTone.core;
        } else if (featureEnabled && isRunning) {
            if (visualKind == FeatureRunVisualKind::TriggerWatch) {
                const qreal pulse = 0.72 + 0.28 * std::sin(animationPhase * M_PI / 36.0);
                triggerAccent = kTriggerWatchAccent;
                triggerAccent.setAlpha(static_cast<int>(pulse * 200.0));
                runAccent = &triggerAccent;
            } else if (visualKind == FeatureRunVisualKind::TriggerCooldown) {
                const qreal pulse = 0.55 + 0.25 * std::sin(animationPhase * M_PI / 24.0);
                triggerAccent = kTriggerCooldownAccent;
                triggerAccent.setAlpha(static_cast<int>(pulse * 165.0));
                runAccent = &triggerAccent;
            }
        }

        paintFeatureEnableToggle(painter, cols.enable, featureEnabled, opt.palette, runAccent);

        const bool isRunningFeature = isRunning;
        const bool holdMode =
            feature && feature->runMode() == FeatureRunMode::Hold;
        const bool hasBlocks = feature && !feature->workflow().blocks().empty();
        const bool runEnabled =
            featureEnabled && (isRunningFeature || (hasBlocks && !holdMode));

        const bool triggerCooldownRow =
            featureEnabled && isRunningFeature && visualKind == FeatureRunVisualKind::TriggerCooldown;
        const qint64 cooldownRemainingMs =
            triggerCooldownRow ? m_panel->triggerCooldownRemainingMs(featureId) : -1;

        if (triggerCooldownRow && cooldownRemainingMs >= 0) {
            paintTriggerCooldownRunButton(painter,
                                          cols.run,
                                          cooldownRemainingMs,
                                          animationPhase,
                                          triggerAccent.isValid() ? triggerAccent : QColor(),
                                          opt.palette);
        } else {
            paintFeatureRunButton(painter,
                                  cols.run,
                                  isRunningFeature,
                                  runEnabled,
                                  opt.palette,
                                  runAccent);
        }

        paintFeatureName(painter,
                         cols.name,
                         opt,
                         featureName,
                         featureEnabled && isRunning && !featureName.isEmpty() && !isDragSource,
                         visualKind,
                         animationPhase);

        QFont secondaryFont = opt.font;
        secondaryFont.setPointSize(qMax(secondaryFont.pointSize() - 1, 8));
        const QColor dragMuted = featureListMutedTextColor(opt.palette);
        QColor modeColor = featureEnabled ? mutedColor : dragMuted;
        if (prismRow) {
            modeColor = prismTone.core;
            modeColor.setAlpha(selected ? 230 : 205);
        } else if (featureEnabled && isRunning && runAccent != nullptr) {
            modeColor = *runAccent;
            modeColor.setAlpha(selected ? 210 : 185);
        }
        drawCellText(painter,
                     cols.mode,
                     modeText,
                     secondaryFont,
                     isDragSource ? dragMuted : modeColor);
        if (hasHotkey) {
            const qreal iconOpacity = isDragSource ? 0.55 : (featureEnabled ? 1.0 : 0.45);
            drawHotkeyBindingInRect(painter, cols.hotkey, hotkeyBinding, iconOpacity);
        } else {
            drawCellText(painter,
                         cols.hotkey,
                         QStringLiteral("\u2014"),
                         secondaryFont,
                         isDragSource ? dragMuted : modeColor);
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

constexpr int kMinFeatureListPanePx = 80;
constexpr int kMinLibraryPaneCollapsedPx = 30;
constexpr int kMinLibraryPaneExpandedPx = 72;
constexpr int kDefaultLibraryPanePx = 120;

} // namespace
FeatureListPanel::FeatureListPanel(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

FeatureListPanel::~FeatureListPanel() = default;
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

    m_headerRow = new ListColumnHeaderWidget(this);
    wireListColumnHeader(m_headerRow, this);

    m_list = new FeatureListWidget(tableFrame);
    m_list->setObjectName(QStringLiteral("featureListView"));
    m_list->setMouseTracking(true);
    if (m_list->viewport()) {
        m_list->viewport()->setMouseTracking(true);
    }
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setUniformItemSizes(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setSpacing(0);
    m_list->setItemDelegate(new FeatureListItemDelegate(this));
    connect(m_list, &FeatureListWidget::featureRowsReordered, this, &FeatureListPanel::onFeatureRowsReordered);
    connect(m_list, &ReorderableListWidget::multiRowsReordered, this, &FeatureListPanel::onFeatureMultiRowsReordered);
    connect(m_list, &FeatureListWidget::featureDropped, this, &FeatureListPanel::featureDropped);
    connect(m_list, &FeatureListWidget::deleteRequested, this, &FeatureListPanel::onRemoveFeature);
    connect(m_list, &FeatureListWidget::copyRequested, this, &FeatureListPanel::onCopyFeature);
    connect(m_list, &FeatureListWidget::pasteRequested, this, &FeatureListPanel::onPasteFeature);
    connect(m_list, &FeatureListWidget::renameRequested, this, &FeatureListPanel::onInlineRenameRequested);
    if (QAbstractItemDelegate* delegate = m_list->itemDelegate()) {
        connect(delegate, &QAbstractItemDelegate::commitData, this, [this](QWidget*) {
            if (m_inlineRenameRow < 0 || !m_list || !m_project) {
                return;
            }
            QListWidgetItem* item = m_list->item(m_inlineRenameRow);
            if (!item) {
                m_inlineRenameRow = -1;
                return;
            }
            const QString name = item->data(kFeatureNameRole).toString().trimmed();
            const int row = m_inlineRenameRow;
            m_inlineRenameRow = -1;
            applyInlineRename(row, name);
        });
        connect(delegate, &QAbstractItemDelegate::closeEditor, this, [this](QWidget*, QAbstractItemDelegate::EndEditHint) {
            m_inlineRenameRow = -1;
            m_renameHotkeyGate.reset();
        });
    }
    m_list->viewport()->installEventFilter(this);

    tableLayout->addWidget(m_headerRow);
    tableLayout->addWidget(m_list, 1);

    m_libraryToggle = new QToolButton(group);
    m_libraryToggle->setObjectName(QStringLiteral("featureLibraryToggle"));
    m_libraryToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_libraryToggle->setArrowType(Qt::RightArrow);
    m_libraryToggle->setCheckable(true);
    m_libraryToggle->setAutoRaise(true);
    m_libraryToggle->setCursor(Qt::PointingHandCursor);
    m_libraryToggle->setFocusPolicy(Qt::NoFocus);
    m_libraryToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_libraryToggle->setToolTip(
        tr("전역 기능 라이브러리입니다. 항목을 더블클릭하면 현재 프로필로 가져옵니다.\n"
           "기능 우클릭 → '라이브러리에 저장'으로 항목을 추가할 수 있습니다."));

    m_libraryDrawerHost = new QWidget();
    m_libraryDrawerHost->setObjectName(QStringLiteral("featureLibraryDrawerHost"));
    m_libraryDrawerHost->setAttribute(Qt::WA_StyledBackground, true);
    m_libraryDrawerHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* drawerLayout = new QVBoxLayout(m_libraryDrawerHost);
    drawerLayout->setContentsMargins(4, 4, 4, 4);
    drawerLayout->setSpacing(0);

    m_libraryList = new FeatureLibraryListWidget(m_libraryDrawerHost);
    m_libraryList->setObjectName(QStringLiteral("featureLibraryList"));
    m_libraryList->setFrameShape(QFrame::NoFrame);
    m_libraryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_libraryList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_libraryList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_libraryList->setUniformItemSizes(true);
    drawerLayout->addWidget(m_libraryList, 1);

    m_libraryPane = new QWidget(group);
    m_libraryPane->setObjectName(QStringLiteral("featureLibraryPane"));
    auto* libraryPaneLayout = new QVBoxLayout(m_libraryPane);
    libraryPaneLayout->setContentsMargins(0, 0, 0, 0);
    libraryPaneLayout->setSpacing(4);
    libraryPaneLayout->addWidget(m_libraryToggle);
    libraryPaneLayout->addWidget(m_libraryDrawerHost, 1);

    tableFrame->setMinimumHeight(kMinFeatureListPanePx);
    m_libraryPane->setMinimumHeight(kMinLibraryPaneCollapsedPx);

    m_featureLibrarySplitter = new QSplitter(Qt::Vertical, group);
    UiResizeHandle::configureSplitter(m_featureLibrarySplitter);
    m_featureLibrarySplitter->addWidget(tableFrame);
    m_featureLibrarySplitter->addWidget(m_libraryPane);
    m_featureLibrarySplitter->setStretchFactor(0, 1);
    m_featureLibrarySplitter->setStretchFactor(1, 0);
    m_featureLibrarySplitter->setCollapsible(0, false);
    m_featureLibrarySplitter->setCollapsible(1, false);
    m_featureLibrarySplitter->setSizes({320, kMinLibraryPaneCollapsedPx});
    connect(m_featureLibrarySplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        if (!m_libraryExpanded) {
            clampFeatureLibrarySplitterSizes();
            return;
        }
        clampFeatureLibrarySplitterSizes();
    });

    connect(m_libraryToggle, &QToolButton::toggled, this, [this](bool checked) {
        setLibraryDrawerExpanded(checked, true);
    });
    connect(m_libraryList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item && m_editControlsEnabled) {
            emit importLibraryEntriesRequested(
                QStringList{item->data(Qt::UserRole).toString()});
        }
    });
    connect(m_libraryList,
            &FeatureLibraryListWidget::featureDroppedOnLibrary,
            this,
            &FeatureListPanel::featureDroppedOnLibrary);
    connect(m_libraryList,
            &FeatureLibraryListWidget::libraryRowsReordered,
            this,
            &FeatureListPanel::onLibraryRowsReordered);
    connect(m_libraryList,
            &ReorderableListWidget::multiRowsReordered,
            this,
            &FeatureListPanel::onLibraryMultiRowsReordered);
    connect(m_libraryList,
            &QListWidget::itemSelectionChanged,
            this,
            &FeatureListPanel::onLibrarySelectionChanged);
    connect(m_libraryList,
            &QListWidget::customContextMenuRequested,
            this,
            &FeatureListPanel::onLibraryContextMenu);
    connect(m_libraryList,
            &FeatureLibraryListWidget::deleteRequested,
            this,
            &FeatureListPanel::onRemoveLibraryEntries);

    groupLayout->addLayout(buttonRow);
    groupLayout->addWidget(m_featureLibrarySplitter, 1);
    outerLayout->addWidget(group);

    {
        QSettings settings;
        const bool expanded =
            settings.value(QStringLiteral("ui/state/featureList/libraryDrawerExpanded"), false).toBool();
        setLibraryDrawerExpanded(expanded, false);
    }
    updateLibraryToggleText();

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(55);
    connect(m_animTimer, &QTimer::timeout, this, &FeatureListPanel::onAnimationTick);
    connect(m_addButton, &QPushButton::clicked, this, &FeatureListPanel::onAddFeature);
    connect(m_removeButton, &QPushButton::clicked, this, &FeatureListPanel::onRemoveFeature);
    connect(m_editButton, &QPushButton::clicked, this, &FeatureListPanel::onEditFeature);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &FeatureListPanel::onSelectionChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onEditFeature();
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, &FeatureListPanel::onContextMenu);
    updateReorderEnabled();
}

void FeatureListPanel::clampFeatureLibrarySplitterSizes() {
    if (!m_featureLibrarySplitter || m_featureLibrarySplitter->count() < 2) {
        return;
    }

    QWidget* featurePane = m_featureLibrarySplitter->widget(0);
    QWidget* libraryPane = m_featureLibrarySplitter->widget(1);
    if (!featurePane || !libraryPane) {
        return;
    }

    updateFeatureLibrarySplitterHandle();

    const int handle = m_featureLibrarySplitter->handleWidth();
    const int total = m_featureLibrarySplitter->height();
    if (total <= 0) {
        return;
    }

    const int available = qMax(0, total - handle);
    const int minTop = qMax(kMinFeatureListPanePx, featurePane->minimumHeight());
    const int minBottom =
        m_libraryExpanded ? qMax(kMinLibraryPaneExpandedPx, libraryPane->minimumHeight())
                          : kMinLibraryPaneCollapsedPx;

    if (!m_libraryExpanded) {
        const int bottomSize = kMinLibraryPaneCollapsedPx;
        const int topSize = qMax(minTop, available - bottomSize);
        QList<int> sizes = m_featureLibrarySplitter->sizes();
        if (sizes.value(0) == topSize && sizes.value(1) == bottomSize) {
            return;
        }
        QSignalBlocker blocker(m_featureLibrarySplitter);
        m_featureLibrarySplitter->setSizes({topSize, bottomSize});
        return;
    }

    if (available < minTop + minBottom) {
        return;
    }

    QList<int> sizes = m_featureLibrarySplitter->sizes();
    int topSize = sizes.value(0, 0);
    int bottomSize = sizes.value(1, 0);
    if (topSize + bottomSize <= 0) {
        return;
    }

    topSize = qBound(minTop, topSize, available - minBottom);
    bottomSize = available - topSize;
    if (bottomSize < minBottom) {
        bottomSize = minBottom;
        topSize = available - bottomSize;
    }

    if (sizes.value(0) == topSize && sizes.value(1) == bottomSize) {
        return;
    }

    QSignalBlocker blocker(m_featureLibrarySplitter);
    m_featureLibrarySplitter->setSizes({topSize, bottomSize});
}

void FeatureListPanel::applyLibraryDrawerVisibility(bool expanded) {
    if (m_libraryDrawerHost) {
        m_libraryDrawerHost->setVisible(expanded);
    }
    if (!m_libraryPane) {
        updateFeatureLibrarySplitterHandle();
        return;
    }
    if (expanded) {
        m_libraryPane->setMinimumHeight(kMinLibraryPaneExpandedPx);
        m_libraryPane->setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        m_libraryPane->setMinimumHeight(kMinLibraryPaneCollapsedPx);
        m_libraryPane->setMaximumHeight(kMinLibraryPaneCollapsedPx);
    }
    updateFeatureLibrarySplitterHandle();
}

void FeatureListPanel::updateFeatureLibrarySplitterHandle() {
    if (!m_featureLibrarySplitter) {
        return;
    }

    if (m_libraryExpanded) {
        m_featureLibrarySplitter->setHandleWidth(UiResizeHandle::kSplitterHandleWidthPx);
        if (QSplitterHandle* handle = m_featureLibrarySplitter->handle(1)) {
            handle->setEnabled(true);
        }
        return;
    }

    m_featureLibrarySplitter->setHandleWidth(0);
    if (QSplitterHandle* handle = m_featureLibrarySplitter->handle(1)) {
        handle->setEnabled(false);
    }
}

void FeatureListPanel::ensureLibraryPaneSizeOnExpand() {
    if (!m_featureLibrarySplitter || !m_libraryExpanded) {
        return;
    }

    const int handle = m_featureLibrarySplitter->handleWidth();
    const int total = m_featureLibrarySplitter->height();
    if (total <= handle) {
        return;
    }

    const int available = total - handle;
    QList<int> sizes = m_featureLibrarySplitter->sizes();
    int bottomSize = sizes.value(1, 0);
    if (bottomSize >= kDefaultLibraryPanePx) {
        clampFeatureLibrarySplitterSizes();
        return;
    }

    bottomSize = qBound(kMinLibraryPaneExpandedPx,
                        kDefaultLibraryPanePx,
                        available - kMinFeatureListPanePx);
    const int topSize = available - bottomSize;

    QSignalBlocker blocker(m_featureLibrarySplitter);
    m_featureLibrarySplitter->setSizes({topSize, bottomSize});
    clampFeatureLibrarySplitterSizes();
}

void FeatureListPanel::setLibraryDrawerExpanded(bool expanded, bool persist) {
    m_libraryExpanded = expanded;
    if (m_libraryToggle) {
        const QSignalBlocker blocker(m_libraryToggle);
        m_libraryToggle->setChecked(expanded);
        m_libraryToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    }

    applyLibraryDrawerVisibility(expanded);
    if (expanded) {
        ensureLibraryPaneSizeOnExpand();
    } else if (m_featureLibrarySplitter) {
        const int handle = m_featureLibrarySplitter->handleWidth();
        const int total = m_featureLibrarySplitter->height();
        if (total > handle) {
            const int available = total - handle;
            const int bottomSize = kMinLibraryPaneCollapsedPx;
            const int topSize = qMax(kMinFeatureListPanePx, available - bottomSize);
            QSignalBlocker blocker(m_featureLibrarySplitter);
            m_featureLibrarySplitter->setSizes({topSize, bottomSize});
        }
    }
    clampFeatureLibrarySplitterSizes();

    if (persist) {
        QSettings settings;
        settings.setValue(QStringLiteral("ui/state/featureList/libraryDrawerExpanded"), expanded);
    }
}

void FeatureListPanel::updateLibraryToggleText() {
    if (!m_libraryToggle) {
        return;
    }
    m_libraryToggle->setText(tr("라이브러리 (%1)").arg(m_libraryEntryCount));
}

void FeatureListPanel::setLibraryEntries(const std::vector<LibraryEntryUi>& entries) {
    if (!m_libraryList) {
        return;
    }
    QStringList selectedIds;
    for (QListWidgetItem* item : m_libraryList->selectedItems()) {
        if (!item) {
            continue;
        }
        const QString id = item->data(Qt::UserRole).toString();
        if (!id.isEmpty()) {
            selectedIds.push_back(id);
        }
    }
    const QString currentId = m_libraryList->currentItem()
                                  ? m_libraryList->currentItem()->data(Qt::UserRole).toString()
                                  : QString();
    if (!currentId.isEmpty() && !selectedIds.contains(currentId)) {
        selectedIds.push_back(currentId);
    }

    m_libraryList->clear();
    QListWidgetItem* firstSelectedItem = nullptr;
    QListWidgetItem* currentItem = nullptr;
    for (const LibraryEntryUi& entry : entries) {
        auto* item = new QListWidgetItem(entry.name, m_libraryList);
        item->setData(Qt::UserRole, entry.id);
        item->setTextAlignment(Qt::AlignCenter);
        const QString tip = entry.templateCount > 0
                                ? tr("%1\n템플릿 %2개\n더블클릭: 현재 프로필로 가져오기")
                                      .arg(entry.name)
                                      .arg(entry.templateCount)
                                : tr("%1\n더블클릭: 현재 프로필로 가져오기").arg(entry.name);
        item->setToolTip(tip);
        if (selectedIds.contains(entry.id)) {
            item->setSelected(true);
            if (!firstSelectedItem) {
                firstSelectedItem = item;
            }
            if (entry.id == currentId) {
                currentItem = item;
            }
        }
    }
    if (currentItem) {
        m_libraryList->setCurrentItem(currentItem);
    } else if (firstSelectedItem) {
        m_libraryList->setCurrentItem(firstSelectedItem);
    }
    m_libraryEntryCount = static_cast<int>(entries.size());
    updateLibraryToggleText();
}

QStringList FeatureListPanel::selectedLibraryEntryIds() const {
    QStringList ids;
    if (!m_libraryList) {
        return ids;
    }
    QList<int> rows;
    for (QListWidgetItem* item : m_libraryList->selectedItems()) {
        if (!item) {
            continue;
        }
        const int row = m_libraryList->row(item);
        if (row < 0) {
            continue;
        }
        const QString id = item->data(Qt::UserRole).toString();
        if (!id.isEmpty()) {
            rows.push_back(row);
            ids.push_back(id);
        }
    }
    if (rows.isEmpty()) {
        return ids;
    }
    QList<QPair<int, QString>> ordered;
    ordered.reserve(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        ordered.push_back({rows[i], ids[i]});
    }
    std::sort(ordered.begin(), ordered.end(), [](const QPair<int, QString>& a, const QPair<int, QString>& b) {
        return a.first < b.first;
    });
    ids.clear();
    for (const auto& pair : ordered) {
        ids.push_back(pair.second);
    }
    return ids;
}

void FeatureListPanel::onRemoveLibraryEntries() {
    const QStringList entryIds = selectedLibraryEntryIds();
    if (entryIds.isEmpty()) {
        return;
    }
    emit deleteLibraryEntriesRequested(entryIds);
}

void FeatureListPanel::onLibraryContextMenu(const QPoint& pos) {
    if (!m_libraryList) {
        return;
    }
    QListWidgetItem* item = m_libraryList->itemAt(pos);
    if (!item) {
        return;
    }
    if (!item->isSelected()) {
        m_libraryList->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
    }
    const QStringList entryIds = selectedLibraryEntryIds();
    if (entryIds.isEmpty()) {
        return;
    }

    QMenu menu(this);
    const bool multiple = entryIds.size() > 1;
    menu.addAction(multiple ? tr("선택 항목 가져오기 (%1)").arg(entryIds.size())
                            : tr("현재 프로필로 가져오기"),
                   this,
                   [this, entryIds]() { emit importLibraryEntriesRequested(entryIds); })
        ->setEnabled(m_editControlsEnabled);
    menu.addSeparator();
    menu.addAction(multiple ? tr("선택 항목 삭제 (%1)").arg(entryIds.size())
                            : tr("라이브러리에서 삭제"),
                   this,
                   [this, entryIds]() { emit deleteLibraryEntriesRequested(entryIds); });
    menu.exec(m_libraryList->mapToGlobal(pos));
}

void FeatureListPanel::setColumnLayout(const FeatureListColumnLayout& layout, bool persist) {
    FeatureListColumnLayout clamped = layout;
    clamped.enableColumnWidth =
        qBound(kMinEnableColumnWidth, layout.enableColumnWidth, kMaxEnableColumnWidth);
    clamped.runColumnWidth =
        qBound(kMinRunColumnWidth, layout.runColumnWidth, kMaxRunColumnWidth);
    clamped.modeColumnWidth =
        qBound(kMinModeColumnWidth, layout.modeColumnWidth, kMaxModeColumnWidth);
    clamped.hotkeyColumnWidth =
        qBound(kMinHotkeyColumnWidth, layout.hotkeyColumnWidth, kMaxHotkeyColumnWidth);
    clamped.rowHeight = UiResizeHandle::clampListRowHeight(layout.rowHeight);
    if (clamped.enableColumnWidth == m_columnLayout.enableColumnWidth
        && clamped.runColumnWidth == m_columnLayout.runColumnWidth
        && clamped.modeColumnWidth == m_columnLayout.modeColumnWidth
        && clamped.hotkeyColumnWidth == m_columnLayout.hotkeyColumnWidth
        && clamped.rowHeight == m_columnLayout.rowHeight) {
        return;
    }
    m_columnLayout = clamped;
    applyColumnLayoutToList();
    if (m_headerRow) {
        m_headerRow->syncToLayout();
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
    settings.setValue(settingsKey + QStringLiteral("/enableColumnWidth"), m_columnLayout.enableColumnWidth);
    settings.setValue(settingsKey + QStringLiteral("/runColumnWidth"), m_columnLayout.runColumnWidth);
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
    layout.enableColumnWidth = qBound(kMinEnableColumnWidth,
                                      readLayoutInt(settings,
                                                    settingsKey + QStringLiteral("/enableColumnWidth"),
                                                    layout.enableColumnWidth),
                                      kMaxEnableColumnWidth);
    layout.runColumnWidth = qBound(kMinRunColumnWidth,
                                   readLayoutInt(settings,
                                                 settingsKey + QStringLiteral("/runColumnWidth"),
                                                 layout.runColumnWidth),
                                   kMaxRunColumnWidth);
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
    layout.rowHeight = UiResizeHandle::clampListRowHeight(
        readLayoutInt(settings,
                      settingsKey + QStringLiteral("/rowHeight"),
                      layout.rowHeight));
    m_restoringColumnLayout = true;
    setColumnLayout(layout, false);
    m_restoringColumnLayout = false;
}
void FeatureListPanel::setRunningFeatureIds(const QSet<QString>& featureIds) {
    m_runningFeatureIds = featureIds;
    if (m_runningFeatureIds.isEmpty()) {
        m_animPhase = 0;
        m_featureRunVisualKinds.clear();
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

void FeatureListPanel::setFeatureRunVisualKinds(const QHash<QString, FeatureRunVisualKind>& kinds) {
    m_featureRunVisualKinds = kinds;
    if (m_list && m_list->viewport()) {
        m_list->viewport()->update();
    }
    updateFeatureEditButtonState();
}

void FeatureListPanel::setTriggerCooldownStates(
    const QHash<QString, FeatureTriggerCooldownState>& states) {
    m_triggerCooldownStates = states;
    if (m_list && m_list->viewport()) {
        m_list->viewport()->update();
    }
}

qint64 FeatureListPanel::triggerCooldownRemainingMs(const QString& featureId) const {
    const auto it = m_triggerCooldownStates.constFind(featureId);
    if (it == m_triggerCooldownStates.constEnd() || it->endsAtEpochMs <= 0) {
        return -1;
    }
    return qMax<qint64>(0, it->endsAtEpochMs - QDateTime::currentMSecsSinceEpoch());
}

int FeatureListPanel::triggerCooldownTotalMs(const QString& featureId) const {
    const auto it = m_triggerCooldownStates.constFind(featureId);
    if (it == m_triggerCooldownStates.constEnd()) {
        return 0;
    }
    return it->totalMs;
}

FeatureRunVisualKind FeatureListPanel::featureRunVisualKind(const QString& featureId) const {
    return m_featureRunVisualKinds.value(featureId, FeatureRunVisualKind::ActiveRun);
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
    if (m_removeButton) {
        m_removeButton->setEnabled(enabled);
    }
    updateListItemEditableFlags();
    updateReorderEnabled();
    updateFeatureEditButtonState();
}

void FeatureListPanel::updateListItemEditableFlags() {
    if (!m_list) {
        return;
    }
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (!item) {
            continue;
        }
        if (m_editControlsEnabled) {
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        } else {
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        }
    }
}

void FeatureListPanel::updateReorderEnabled() {
    if (!m_list) {
        return;
    }
    const bool enabled = m_editControlsEnabled && m_runningFeatureIds.isEmpty();
    m_list->setReorderEnabled(enabled);
    if (m_libraryList) {
        m_libraryList->setTransferEnabled(enabled);
    }
}

const Feature* FeatureListPanel::projectFeatureById(const QString& featureId) const {
    if (!m_project || featureId.isEmpty()) {
        return nullptr;
    }
    return m_project->featureById(featureId.toStdString());
}

QList<int> FeatureListPanel::selectedRows() const {
    QList<int> rows;
    if (!m_list) {
        return rows;
    }
    const QList<QListWidgetItem*> items = m_list->selectedItems();
    rows.reserve(items.size());
    for (QListWidgetItem* item : items) {
        if (!item) {
            continue;
        }
        rows.push_back(m_list->row(item));
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    return rows;
}

bool FeatureListPanel::enableToggleHitTest(int row, const QPoint& viewportPos) const {
    if (!m_list || row < 0 || row >= m_list->count()) {
        return false;
    }
    QListWidgetItem* item = m_list->item(row);
    if (!item) {
        return false;
    }
    const QRect itemRect = m_list->visualItemRect(item);
    const FeatureListColumnRects cols = featureListColumnRects(itemRect, m_columnLayout);
    return featureEnableToggleHitRect(cols.enable).contains(viewportPos);
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

void FeatureListPanel::toggleFeatureEnabled(int row) {
    if (!m_list || !m_project || row < 0 || row >= m_list->count()) {
        return;
    }
    Feature* feature = m_project->featureAt(row);
    if (!feature) {
        return;
    }
    emit mutationAboutToCommit(QStringLiteral("feature-toggle-enabled"));
    feature->setEnabled(!feature->enabled());
    if (QListWidgetItem* item = m_list->item(row)) {
        configureListItem(item, *feature);
    }
    if (m_list->viewport()) {
        m_list->viewport()->update();
    }
    const QString featureId = QString::fromStdString(feature->id());
    emit featureEnabledChanged(featureId, feature->enabled());
    emit hotkeysChanged();
    emit projectModified();
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
    const bool holdMode = feature->runMode() == FeatureRunMode::Hold;
    const bool hasBlocks = !feature->workflow().blocks().empty();
    if (!feature->enabled()) {
        return;
    }
    if (!running && (holdMode || !hasBlocks)) {
        return;
    }
    emit featureRunRequested(featureId);
}

void FeatureListPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    clampFeatureLibrarySplitterSizes();
}

bool FeatureListPanel::eventFilter(QObject* watched, QEvent* event) {
    if (m_list && watched == m_list->viewport() && event->type() == QEvent::MouseButtonRelease) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QListWidgetItem* item = m_list->itemAt(mouseEvent->pos());
            if (item) {
                const int row = m_list->row(item);
                if (enableToggleHitTest(row, mouseEvent->pos())) {
                    toggleFeatureEnabled(row);
                    return true;
                }
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
        emit mutationAboutToCommit(QStringLiteral("feature-reorder"));
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

void FeatureListPanel::onFeatureMultiRowsReordered(const QList<int>& selectedRows, int insertIndex) {
    if (!m_project || selectedRows.isEmpty()) {
        if (m_list) {
            refresh();
        }
        return;
    }
    std::vector<int> rows;
    rows.reserve(static_cast<size_t>(selectedRows.size()));
    for (int row : selectedRows) {
        rows.push_back(row);
    }
    emit mutationAboutToCommit(QStringLiteral("feature-reorder-multi"));
    m_project->moveFeatures(rows, insertIndex);
    emit projectModified();
    refresh();
    if (m_list) {
        m_list->clearSelection();
        int dest = insertIndex;
        for (int row : selectedRows) {
            if (row < insertIndex) {
                --dest;
            }
        }
        dest = qBound(0, dest, m_list->count());
        for (int i = 0; i < selectedRows.size() && dest + i < m_list->count(); ++i) {
            if (QListWidgetItem* item = m_list->item(dest + i)) {
                item->setSelected(true);
            }
        }
        if (dest < m_list->count()) {
            m_list->setCurrentRow(dest);
        }
    }
}

void FeatureListPanel::onLibraryRowsReordered(int fromRow, int toRow) {
    if (!m_libraryList || fromRow == toRow || !m_editControlsEnabled) {
        return;
    }
    emit libraryEntriesReordered(fromRow, toRow);
    if (fromRow >= 0 && fromRow < m_libraryList->count()) {
        m_libraryList->setCurrentRow(toRow);
    }
}

void FeatureListPanel::onLibraryMultiRowsReordered(const QList<int>& selectedRows, int insertIndex) {
    if (!m_libraryList || selectedRows.isEmpty() || !m_editControlsEnabled) {
        return;
    }
    emit libraryEntriesMultiReordered(selectedRows, insertIndex);
}

void FeatureListPanel::onLibrarySelectionChanged() {
    if (!m_libraryList) {
        return;
    }

    const QString entryId = m_libraryList->currentItem()
                                ? m_libraryList->currentItem()->data(Qt::UserRole).toString()
                                : QString();
    if (!entryId.isEmpty() && m_list) {
        const QSignalBlocker blocker(m_list);
        m_list->setCurrentRow(-1);
        m_list->clearSelection();
    }
    emit libraryEntrySelected(entryId);
}

void FeatureListPanel::onAnimationTick() {
    m_animPhase = (m_animPhase + 1) % 96;
    if (m_list && m_list->viewport()) {
        m_list->viewport()->update();
    }
}
void FeatureListPanel::setActiveProfileId(const QString& profileId) {
    m_lastSelectedFeatureId.clear();
    if (m_list) {
        m_list->setActiveProfileId(profileId);
    }
}

void FeatureListPanel::setProject(Project* project, bool refreshList) {
    m_project = project;
    if (refreshList) {
        refresh();
    }
}
void FeatureListPanel::configureListItem(QListWidgetItem* item, const Feature& feature) {
    const QString featureName = QString::fromStdString(feature.name());
    const QString modeText = featureRunModeCompact(feature.runMode(), feature.repeatCount());
    const HotkeyBinding& hotkey = feature.hotkey();
    item->setText(featureName);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    item->setSizeHint(QSize(0, m_columnLayout.rowHeight));
    item->setData(kFeatureIdRole, QString::fromStdString(feature.id()));
    item->setData(kFeatureNameRole, featureName);
    item->setData(kRunModeDisplayRole, modeText);
    item->setData(kHotkeyVirtualKeyRole, hotkey.virtualKey);
    item->setData(kHotkeyCtrlRole, hotkey.ctrl);
    item->setData(kHotkeyAltRole, hotkey.alt);
    item->setData(kHotkeyShiftRole, hotkey.shift);
    if (m_editControlsEnabled) {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    } else {
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    }
    QString tooltip = featureName;
    tooltip += QStringLiteral("\n") + tr("동작: %1").arg(featureRunModeLabel(feature.runMode()));
    if (feature.runMode() == FeatureRunMode::RepeatCount) {
        tooltip += tr(" (%1회)").arg(feature.repeatCount());
    }
    if (feature.runMode() == FeatureRunMode::Trigger) {
        tooltip += QStringLiteral("\n")
                   + tr("쿨다운: %1초").arg(triggerCooldownSecondsFromMs(feature.triggerCooldownMs()));
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
    tooltip += QStringLiteral("\n") + tr("사용: %1").arg(feature.enabled() ? tr("켜짐") : tr("꺼짐"));
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
    emit mutationAboutToCommit(QStringLiteral("feature-add"));
    m_project->addFeature(std::string{});
    refresh();
    const int index = static_cast<int>(m_project->features().size()) - 1;
    m_list->setCurrentRow(index);
    if (!editFeatureAt(index)) {
        m_project->removeFeature(index);
        refresh();
        emit selectionChanged();
        emit projectModified();
    }
}
void FeatureListPanel::onCopyFeature() {
    if (!m_project) {
        return;
    }
    const int index = selectedIndex();
    if (index < 0) {
        return;
    }
    const Feature* feature = m_project->featureAt(index);
    if (!feature) {
        return;
    }
    m_clipboardFeature = feature->clone();
}
void FeatureListPanel::onPasteFeature() {
    if (!m_project || !m_editControlsEnabled || !m_clipboardFeature) {
        return;
    }

    auto duplicate = m_clipboardFeature->duplicateAsNewInstance();
    duplicate->setName(tr("%1 복사").arg(QString::fromStdString(duplicate->name())).toStdString());

    const int selected = selectedIndex();
    const int insertIndex = selected >= 0 ? selected + 1 : static_cast<int>(m_project->features().size());
    emit mutationAboutToCommit(QStringLiteral("feature-paste"));
    m_project->insertFeature(insertIndex, std::move(duplicate));

    refresh();
    m_list->setCurrentRow(insertIndex);
    emit selectionChanged();
    emit projectModified();
}
void FeatureListPanel::onRemoveFeature() {
    if (!m_project || !m_editControlsEnabled) {
        return;
    }
    const QList<int> rows = selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    const int restoreRow = qMax(0, rows.front() - 1);
    emit mutationAboutToCommit(QStringLiteral("feature-remove"));
    for (auto it = rows.crbegin(); it != rows.crend(); ++it) {
        m_project->removeFeature(*it);
    }
    refresh();
    if (m_list && m_list->count() > 0) {
        m_list->setCurrentRow(qMin(restoreRow, m_list->count() - 1));
    }
    emit selectionChanged();
    emit projectModified();
    emit hotkeysChanged();
}
bool FeatureListPanel::isTriggerSettingsEditableAt(int index) const {
    if (!m_project || index < 0 || index >= static_cast<int>(m_project->features().size())) {
        return false;
    }
    const Feature* feature = m_project->featureAt(index);
    if (!feature || feature->runMode() != FeatureRunMode::Trigger) {
        return false;
    }
    const QString featureId = QString::fromStdString(feature->id());
    if (!isFeatureRunning(featureId)) {
        return false;
    }
    const FeatureRunVisualKind kind = featureRunVisualKind(featureId);
    return kind == FeatureRunVisualKind::TriggerWatch || kind == FeatureRunVisualKind::TriggerCooldown;
}

bool FeatureListPanel::isFeatureEditableAt(int index) const {
    if (m_editControlsEnabled) {
        return true;
    }
    if (isTriggerSettingsEditableAt(index)) {
        return true;
    }
    if (!m_project || index < 0 || index >= static_cast<int>(m_project->features().size())) {
        return false;
    }
    const Feature* feature = m_project->featureAt(index);
    if (!feature) {
        return false;
    }
    return !isFeatureRunning(QString::fromStdString(feature->id()));
}

void FeatureListPanel::updateFeatureEditButtonState() {
    if (!m_editButton) {
        return;
    }
    m_editButton->setEnabled(isFeatureEditableAt(selectedIndex()));
}

bool FeatureListPanel::editFeatureAt(int index) {
    if (!isFeatureEditableAt(index)) {
        return false;
    }
    if (!m_project || index < 0 || index >= static_cast<int>(m_project->features().size())) {
        return false;
    }
    Feature* feature = m_project->featureAt(index);
    if (!feature) {
        return false;
    }
    const bool isNewFeature = feature->name().empty();
    if (isNewFeature) {
        applyLastFeatureEditSettings(*feature);
    }
    const HotkeyBinding previousHotkey = feature->hotkey();
    const bool previousHotkeyAllowExtraModifiers = feature->hotkeyAllowExtraModifiers();
    const bool previousEnabled = feature->enabled();
    const FeatureRunMode previousMode = feature->runMode();
    FeatureEditDialog dialog(QString::fromStdString(feature->name()),
                             feature->enabled(),
                             feature->hotkey(),
                             feature->hotkeyAllowExtraModifiers(),
                             feature->captureTargetScope(),
                             feature->requireScopedTargetForeground(),
                             feature->runMode(),
                             feature->repeatCount(),
                             feature->infiniteExitAfterConsecutiveMisses(),
                             feature->userInputInterruptMode(),
                             feature->pointerVisualFeedback(),
                             feature->restoreMousePositionOnEnd(),
                             feature->lockMouseDuringFirstLoopCount(),
                             feature->unlockMouseOnBlockFailureCount(),
                             feature->roiCorrection(),
                             feature->roiCorrectionExpandPercent(),
                             feature->editFirstTemplateRoiOnStart(),
                             feature->triggerCooldownMs(),
                             feature->loopIntervalMs(),
                             feature->loopIntervalRandomRange(),
                             feature->loopIntervalMinMs(),
                             feature->loopIntervalMaxMs(),
                             m_project,
                             feature->id(),
                             this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    emit mutationAboutToCommit(QStringLiteral("feature-edit"));
    feature->setName(dialog.featureName().toStdString());
    feature->setEnabled(dialog.featureEnabled());
    feature->setHotkey(dialog.hotkey());
    feature->setHotkeyAllowExtraModifiers(dialog.hotkeyAllowExtraModifiers());
    feature->setCaptureTargetScope(dialog.captureTargetScope());
    feature->setRequireScopedTargetForeground(dialog.requireScopedTargetForeground());
    feature->setRunMode(dialog.runMode());
    feature->setRepeatCount(dialog.repeatCount());
    feature->setInfiniteExitAfterConsecutiveMisses(dialog.infiniteExitAfterConsecutiveMisses());
    feature->setUserInputInterruptMode(dialog.userInputInterruptMode());
    feature->setPointerVisualFeedback(dialog.pointerVisualFeedback());
    feature->setRestoreMousePositionOnEnd(dialog.restoreMousePositionOnEnd());
    feature->setLockMouseDuringFirstLoopCount(dialog.lockMouseDuringFirstLoopCount());
    feature->setUnlockMouseOnBlockFailureCount(dialog.unlockMouseOnBlockFailureCount());
    feature->setRoiCorrection(dialog.roiCorrection());
    feature->setRoiCorrectionExpandPercent(dialog.roiCorrectionExpandPercent());
    feature->setEditFirstTemplateRoiOnStart(dialog.editFirstTemplateRoiOnStart());
    feature->setTriggerCooldownMs(dialog.triggerCooldownMs());
    feature->setLoopIntervalMs(dialog.loopIntervalMs());
    feature->setLoopIntervalRandomRange(dialog.loopIntervalRandomRange());
    feature->setLoopIntervalMinMs(dialog.loopIntervalMinMs());
    feature->setLoopIntervalMaxMs(dialog.loopIntervalMaxMs());
    saveLastFeatureEditSettings(*feature);
    refresh();
    m_list->setCurrentRow(index);
    emit projectModified();
    if (previousHotkey != feature->hotkey() || previousMode != feature->runMode()
        || previousHotkeyAllowExtraModifiers != feature->hotkeyAllowExtraModifiers()
        || previousEnabled != feature->enabled()) {
        emit hotkeysChanged();
    }
    if (previousEnabled != feature->enabled()) {
        emit featureEnabledChanged(QString::fromStdString(feature->id()), feature->enabled());
    }
    return true;
}
void FeatureListPanel::onEditFeature() {
    editFeatureAt(selectedIndex());
}

void FeatureListPanel::onInlineRenameRequested() {
    if (!m_editControlsEnabled || !m_list || !m_project) {
        return;
    }
    QListWidgetItem* item = m_list->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsEditable)) {
        return;
    }
    m_inlineRenameRow = m_list->row(item);
    m_renameHotkeyGate = std::make_unique<FeatureHotkeyGateScope>();
    m_list->editItem(item);
}

void FeatureListPanel::applyInlineRename(int row, const QString& name) {
    if (!m_project || row < 0 || name.isEmpty()) {
        return;
    }
    Feature* feature = m_project->featureAt(row);
    if (!feature) {
        return;
    }
    const QString previous = QString::fromStdString(feature->name());
    if (previous == name) {
        return;
    }
    emit mutationAboutToCommit(QStringLiteral("feature-rename"));
    feature->setName(name.toStdString());
    if (QListWidgetItem* item = m_list->item(row)) {
        const QSignalBlocker blocker(m_list);
        configureListItem(item, *feature);
    }
    emit projectModified();
}

void FeatureListPanel::onContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_list->itemAt(pos);
    if (!item) {
        return;
    }
    if (!item->isSelected()) {
        m_list->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
    }
    const QString featureId = item->data(kFeatureIdRole).toString();
    Feature* feature =
        m_project ? m_project->featureById(featureId.toStdString()) : nullptr;
    QMenu menu(this);
    const bool running = feature && isFeatureRunning(featureId);
    const bool canRun = feature && feature->enabled() && !running
                        && feature->runMode() != FeatureRunMode::Hold
                        && !feature->workflow().blocks().empty();
    const bool canSaveToLibrary =
        feature && m_editControlsEnabled && !running && !feature->workflow().blocks().empty();
    if (feature) {
        if (feature->enabled()) {
            menu.addAction(tr("사용 안 함"), this, [this, row = m_list->row(item)]() {
                toggleFeatureEnabled(row);
            });
        } else {
            menu.addAction(tr("사용"), this, [this, row = m_list->row(item)]() {
                toggleFeatureEnabled(row);
            });
        }
    }
    if (running) {
        menu.addAction(tr("중지"), this, [this, featureId]() {
            emit featureRunRequested(featureId);
        });
    } else if (canRun) {
        menu.addAction(tr("실행"), this, [this, featureId]() {
            emit featureRunRequested(featureId);
        });
    }
    menu.addAction(tr("라이브러리에 저장"), this, [this, featureId]() {
        emit saveFeatureToLibraryRequested(featureId);
    })
        ->setEnabled(canSaveToLibrary);
    menu.addAction(tr("라이브러리에서 가져오기"), this, [this]() {
        emit importFeatureFromLibraryRequested();
    })
        ->setEnabled(m_editControlsEnabled);
    menu.addAction(tr("이름 바꾸기"), this, &FeatureListPanel::onInlineRenameRequested)
        ->setEnabled(m_editControlsEnabled);
    menu.addAction(tr("편집"), this, &FeatureListPanel::onEditFeature)
        ->setEnabled(isFeatureEditableAt(m_list->row(item)));
    menu.addAction(tr("복사"), this, &FeatureListPanel::onCopyFeature);
    menu.addAction(tr("붙여넣기"), this, &FeatureListPanel::onPasteFeature)
        ->setEnabled(m_editControlsEnabled && m_clipboardFeature != nullptr);
    menu.addAction(tr("삭제"), this, &FeatureListPanel::onRemoveFeature);
    menu.exec(m_list->mapToGlobal(pos));
}

void FeatureListPanel::wireListColumnHeader(ListColumnHeaderWidget* header, FeatureListPanel* panel) {
    if (!header || !panel) {
        return;
    }

    header->setObjectName(QStringLiteral("featureListHeaderRow"));
    header->setToolTip(
        QObject::tr("헤더 구분선을 드래그해 열 너비·줄 높이를 조절합니다.\n"
                    "· 사용|▶ 경계: 사용 열 너비\n"
                    "· ▶|기능 경계: 실행 열 너비\n"
                    "· 기능|방식 경계: 방식 열 너비\n"
                    "· 방식|키 경계: 키 열 너비\n"
                    "· 헤더 아래 가장자리: 줄 높이"));

    header->setRowHeightProvider([panel] { return panel->columnLayout().rowHeight; });

    header->setPaintLabelsProvider([panel](ListColumnHeaderWidget::PaintContext& ctx) {
        const FeatureListColumnEdges edges =
            featureListColumnEdges(ctx.rect, panel->columnLayout());
        QFont headerFont = ctx.painter->font();
        headerFont.setPointSize(qMax(headerFont.pointSize(), 9));
        headerFont.setBold(true);
        ctx.painter->setFont(headerFont);
        ctx.painter->setPen(ListColumnHeaderWidget::headerTextColor(ctx.palette));
        const Qt::Alignment headerAlign = Qt::AlignHCenter | Qt::AlignVCenter;
        ctx.painter->drawText(edges.cols.enable, headerAlign, FeatureListPanel::tr("사용"));
        ctx.painter->drawText(edges.cols.run, headerAlign, QStringLiteral("\u25b6"));
        ctx.painter->drawText(edges.cols.name, headerAlign, FeatureListPanel::tr("기능"));
        ctx.painter->drawText(edges.cols.mode, headerAlign, FeatureListPanel::tr("방식"));
        ctx.painter->drawText(edges.cols.hotkey, headerAlign, FeatureListPanel::tr("키"));
    });

    header->setDividerXsProvider([panel](const QRect& rect) {
        const FeatureListColumnEdges edges = featureListColumnEdges(rect, panel->columnLayout());
        return featureListDividerXs(edges);
    });

    header->setDividerHitProvider([panel](const QPoint& pos, const QRect& rect) {
        const FeatureListColumnEdges edges = featureListColumnEdges(rect, panel->columnLayout());
        return featureListDividerHandleAt(pos, edges);
    });

    header->setApplyDragProvider([panel, header](int handleId, int deltaX, int deltaY, const QPoint&) {
        if (handleId == -1) {
            FeatureListColumnLayout layout = panel->m_headerDragStartLayout;
            layout.rowHeight =
                UiResizeHandle::clampListRowHeight(panel->m_headerDragStartLayout.rowHeight + deltaY);
            panel->setColumnLayout(layout, true);
            header->syncToLayout();
            return;
        }

        FeatureListColumnLayout layout = panel->m_headerDragStartLayout;
        switch (static_cast<FeatureListResizeHandleId>(handleId)) {
        case FeatureListResizeHandleId::Run: {
            int enableWidth = panel->m_headerDragStartLayout.enableColumnWidth;
            int runWidth = panel->m_headerDragStartLayout.runColumnWidth;
            int newEnable = qBound(kMinEnableColumnWidth, enableWidth + deltaX, kMaxEnableColumnWidth);
            int actualDelta = newEnable - enableWidth;
            int newRun = qBound(kMinRunColumnWidth, runWidth - actualDelta, kMaxRunColumnWidth);
            actualDelta = runWidth - newRun;
            newEnable = qBound(kMinEnableColumnWidth, enableWidth + actualDelta, kMaxEnableColumnWidth);
            layout.enableColumnWidth = newEnable;
            layout.runColumnWidth = newRun;
            break;
        }
        case FeatureListResizeHandleId::RunName:
            layout.runColumnWidth = qBound(kMinRunColumnWidth,
                                           panel->m_headerDragStartLayout.runColumnWidth + deltaX,
                                           kMaxRunColumnWidth);
            break;
        case FeatureListResizeHandleId::Mode:
            layout.modeColumnWidth = qBound(kMinModeColumnWidth,
                                            panel->m_headerDragStartLayout.modeColumnWidth - deltaX,
                                            kMaxModeColumnWidth);
            break;
        case FeatureListResizeHandleId::Hotkey:
            layout.hotkeyColumnWidth = qBound(kMinHotkeyColumnWidth,
                                              panel->m_headerDragStartLayout.hotkeyColumnWidth - deltaX,
                                              kMaxHotkeyColumnWidth);
            break;
        default:
            return;
        }
        panel->setColumnLayout(layout, true);
        header->syncToLayout();
    });

    QObject::connect(header, &ListColumnHeaderWidget::dividerDragStarted, panel, [panel]() {
        panel->m_headerDragStartLayout = panel->columnLayout();
    });

    QObject::connect(
        panel,
        &FeatureListPanel::columnLayoutChanged,
        header,
        &ListColumnHeaderWidget::syncToLayout);
}

void FeatureListPanel::onSelectionChanged() {
    if (Feature* feature = selectedFeature()) {
        m_lastSelectedFeatureId = QString::fromStdString(feature->id());
    }
    if (m_libraryList) {
        const QSignalBlocker blocker(m_libraryList);
        m_libraryList->setCurrentRow(-1);
        m_libraryList->clearSelection();
    }
    updateFeatureEditButtonState();
    emit selectionChanged();
}
