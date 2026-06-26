#pragma once

#include <QPixmap>
#include <QString>
#include <vector>

struct IfBranchBlockDisplay {
    QString type;
    QString summary;
    QPixmap thumbnail;
};

struct IfBlockDisplay {
    int blockRow = 0;
    QString conditionText;
    std::vector<IfBranchBlockDisplay> thenBlocks;
    std::vector<IfBranchBlockDisplay> elseBlocks;
};

enum class BlockListRowKind {
    MainBlock,
    LoopRegionHeader,
    IfHeader,
    IfBranchHeader,
    IfBranchBlock,
};

struct BlockListRowMeta {
    BlockListRowKind kind = BlockListRowKind::MainBlock;
    int mainBlockRow = -1;
    QString loopRegionId;
    QString ifConditionText;
    bool isThenBranch = true;
    int branchBlockIndex = -1;
};
