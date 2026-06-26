#pragma once

#include "core/workflow/Block.h"

#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QPoint>
#include <QPushButton>
#include <QScreen>
#include <QSize>
#include <QString>

inline QString blockTypeDisplayName(BlockType type) {
    switch (type) {
    case BlockType::ImageFind:
        return QStringLiteral("템플릿 매칭");
    case BlockType::Click:
        return QStringLiteral("마우스");
    case BlockType::KeyPress:
        return QStringLiteral("키보드");
    case BlockType::Wait:
        return QStringLiteral("딜레이");
    case BlockType::If:
        return QStringLiteral("만약");
    case BlockType::Loop:
        return QStringLiteral("구간 반복");
    case BlockType::Comment:
        return QStringLiteral("주석");
    }
    return QStringLiteral("알 수 없음");
}

inline QString blockTypeWorkflowActionName(BlockType type) {
    switch (type) {
    case BlockType::ImageFind:
        return QStringLiteral("템플릿");
    case BlockType::If:
        return QStringLiteral("분기");
    case BlockType::Loop:
        return QStringLiteral("반복");
    default:
        return blockTypeDisplayName(type);
    }
}

inline void localizeDialogButtons(QDialogButtonBox* box) {
    if (!box) {
        return;
    }
    if (QPushButton* ok = box->button(QDialogButtonBox::Ok)) {
        ok->setText(QStringLiteral("확인"));
    }
    if (QPushButton* cancel = box->button(QDialogButtonBox::Cancel)) {
        cancel->setText(QStringLiteral("취소"));
    }
}

inline void positionDialogNearGlobalPoint(QDialog& dialog, const QPoint& globalPos, int offset = 16) {
    dialog.adjustSize();
    const QSize size = dialog.size();
    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }

    const QRect avail = screen->availableGeometry();
    int x = globalPos.x() + offset;
    int y = globalPos.y() + offset;
    if (x + size.width() > avail.right()) {
        x = globalPos.x() - size.width() - offset;
    }
    if (y + size.height() > avail.bottom()) {
        y = globalPos.y() - size.height() - offset;
    }
    x = qBound(avail.left(), x, qMax(avail.left(), avail.right() - size.width() + 1));
    y = qBound(avail.top(), y, qMax(avail.top(), avail.bottom() - size.height() + 1));
    dialog.move(x, y);
}
