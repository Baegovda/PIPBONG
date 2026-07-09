#include "ui/ProfileListWidget.h"

#include "ui/FeatureDragMime.h"
#include "ui/UiThemeColors.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QListWidgetItem>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyledItemDelegate>

namespace {

constexpr int kDefaultProfileRole = Qt::UserRole + 2;

class ProfileListItemDelegate : public QStyledItemDelegate {
public:
    explicit ProfileListItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!index.data(kDefaultProfileRole).toBool()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QPalette pal = opt.widget ? opt.widget->palette() : QApplication::palette();
        const bool selected = opt.state.testFlag(QStyle::State_Selected);
        const QColor textColor = primaryContentTextColor(pal, selected);
        const QColor accent = textColor.lightness() < 128 ? QColor(0x64, 0xb5, 0xf6) : QColor(0x1e, 0x88, 0xe5);
        const QColor fill = selected ? pal.color(QPalette::Highlight)
                                     : (pal.color(QPalette::Button).isValid() ? pal.color(QPalette::Button)
                                                                              : pal.color(QPalette::AlternateBase));
        const QColor border = selected ? accent : fill.darker(108);

        QRect rowRect = opt.rect.adjusted(2, 1, -2, -1);
        QPainterPath path;
        path.addRoundedRect(rowRect, 6, 6);
        painter->fillPath(path, fill);
        painter->setPen(QPen(border, 1));
        painter->drawPath(path);

        const int accentBarWidth = 3;
        QRect accentBar(rowRect.left() + 1, rowRect.top() + 4, accentBarWidth, rowRect.height() - 8);
        painter->fillRect(accentBar, accent);

        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const int iconSize = 20;
        const int left = rowRect.left() + accentBarWidth + 8;
        const QRect iconRect(left, rowRect.center().y() - iconSize / 2, iconSize, iconSize);
        if (!icon.isNull()) {
            icon.paint(painter, iconRect);
        }

        const QString badgeText = QStringLiteral("시스템 고정");
        QFont badgeFont = opt.font;
        badgeFont.setBold(true);
        badgeFont.setPointSizeF(qMax(8.0, badgeFont.pointSizeF() - 0.5));
        const QFontMetrics badgeFm(badgeFont);
        const int badgeHorizontalPad = 8;
        const int badgeWidth = badgeFm.horizontalAdvance(badgeText) + badgeHorizontalPad * 2;
        const int badgeHeight = qMax(16, badgeFm.height() + 4);
        QRect badgeRect(rowRect.right() - badgeWidth - 6,
                        rowRect.center().y() - badgeHeight / 2,
                        badgeWidth,
                        badgeHeight);
        const QColor badgeTextColor = accent.lightness() < 140 ? QColor(Qt::white) : QColor(0x10, 0x18, 0x20);
        painter->setPen(Qt::NoPen);
        painter->setBrush(accent);
        painter->drawRoundedRect(badgeRect, badgeHeight / 2.0, badgeHeight / 2.0);
        painter->setPen(badgeTextColor);
        painter->setFont(badgeFont);
        painter->drawText(badgeRect, Qt::AlignCenter, badgeText);

        const int textLeft = iconRect.right() + 8;
        const int textRight = badgeRect.left() - 6;
        QRect textRect(textLeft, rowRect.top(), qMax(0, textRight - textLeft), rowRect.height());
        QFont nameFont = opt.font;
        nameFont.setBold(true);
        painter->setFont(nameFont);
        painter->setPen(textColor);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("기본"));

        if (selected && opt.state.testFlag(QStyle::State_HasFocus)) {
            QStyleOptionFocusRect focus;
            focus.rect = rowRect.adjusted(1, 1, -1, -1);
            focus.state = opt.state;
            focus.palette = pal;
            if (const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style()) {
                style->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, painter, opt.widget);
            }
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        if (index.data(kDefaultProfileRole).toBool()) {
            size.setHeight(qMax(size.height(), 30));
        }
        return size;
    }
};

} // namespace

ProfileListWidget::ProfileListWidget(QWidget* parent)
    : ReorderableListWidget(parent) {
    m_itemDelegate = new ProfileListItemDelegate(this);
    setItemDelegate(m_itemDelegate);
}

void ProfileListWidget::setDefaultProfileId(const QString& profileId) {
    m_defaultProfileId = profileId;
    viewport()->update();
}

void ProfileListWidget::setFeatureDropEnabled(bool enabled) {
    m_featureDropEnabled = enabled;
    if (!enabled) {
        updateFeatureDropHighlight(-1);
    }
}

bool ProfileListWidget::isDefaultProfileItem(QListWidgetItem* item) const {
    return item && item->data(kDefaultProfileRole).toBool();
}

bool ProfileListWidget::canStartDragFromRow(int row) const {
    return !isDefaultProfileItem(item(row));
}

int ProfileListWidget::minimumDropInsertionIndex() const {
    return count() > 0 && isDefaultProfileItem(item(0)) ? 1 : 0;
}

void ProfileListWidget::mousePressEvent(QMouseEvent* event) {
    if (event && event->button() == Qt::LeftButton) {
        if (QListWidgetItem* hit = itemAt(event->position().toPoint())) {
            setCurrentItem(hit);
        }
    }
    ReorderableListWidget::mousePressEvent(event);
}

void ProfileListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (m_featureDropEnabled && FeatureDragMime::accepts(event->mimeData()) && event->source() != this) {
        const int highlightRow = row(itemAt(event->position().toPoint()));
        updateFeatureDropHighlight(highlightRow);
        event->acceptProposedAction();
        return;
    }
    updateFeatureDropHighlight(-1);
    ReorderableListWidget::dragEnterEvent(event);
}

void ProfileListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (m_featureDropEnabled && FeatureDragMime::accepts(event->mimeData()) && event->source() != this) {
        const int highlightRow = row(itemAt(event->position().toPoint()));
        updateFeatureDropHighlight(highlightRow);
        event->acceptProposedAction();
        return;
    }
    updateFeatureDropHighlight(-1);
    ReorderableListWidget::dragMoveEvent(event);
}

void ProfileListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    updateFeatureDropHighlight(-1);
    ReorderableListWidget::dragLeaveEvent(event);
}

QString ProfileListWidget::profileIdAt(const QPoint& pos) const {
    QListWidgetItem* hit = itemAt(pos);
    if (!hit) {
        return {};
    }
    return hit->data(Qt::UserRole).toString();
}

void ProfileListWidget::updateFeatureDropHighlight(int row) {
    if (m_featureDropHighlightRow == row) {
        return;
    }
    if (m_featureDropHighlightRow >= 0 && m_featureDropHighlightRow < count()) {
        if (QListWidgetItem* previous = item(m_featureDropHighlightRow)) {
            previous->setData(Qt::UserRole + 1, {});
        }
    }
    m_featureDropHighlightRow = row;
    if (row >= 0 && row < count()) {
        if (QListWidgetItem* current = item(row)) {
            current->setData(Qt::UserRole + 1, QStringLiteral("featureDropHover"));
        }
    }
    viewport()->update();
}

void ProfileListWidget::dropEvent(QDropEvent* event) {
    updateFeatureDropHighlight(-1);

    if (m_featureDropEnabled && FeatureDragMime::accepts(event->mimeData()) && event->source() != this) {
        const QString profileId = profileIdAt(event->position().toPoint());
        if (profileId.isEmpty()) {
            event->ignore();
            return;
        }

        const FeatureDragMime::Payload payload = FeatureDragMime::parse(event->mimeData());
        if (!payload.isValid()) {
            event->ignore();
            return;
        }

        if (payload.source == FeatureDragMime::Source::Profile
            && payload.profileId == profileId) {
            event->ignore();
            return;
        }

        emit featureDroppedOnProfile(profileId, event->mimeData());
        event->setDropAction(payload.source == FeatureDragMime::Source::Library ? Qt::CopyAction
                                                                              : Qt::MoveAction);
        event->accept();
        return;
    }

    ReorderableListWidget::dropEvent(event);
}
