#pragma once

#include <QString>

enum class BlockListRowKind {
    MainBlock,
    LoopRegionHeader,
};

struct BlockListRowMeta {
    BlockListRowKind kind = BlockListRowKind::MainBlock;
    int mainBlockRow = -1;
    QString loopRegionId;
};
