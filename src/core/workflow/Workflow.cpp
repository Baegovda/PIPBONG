#include "core/workflow/Workflow.h"

#include <QUuid>

#include <algorithm>

namespace {

std::string newLoopRegionId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

bool regionsOverlap(const WorkflowLoopRegion& a, const WorkflowLoopRegion& b) {
    return a.startIndex <= b.endIndex && b.startIndex <= a.endIndex;
}

} // namespace

const WorkflowLoopRegion* Workflow::loopRegionStartingAt(int blockIndex) const {
    for (const WorkflowLoopRegion& region : m_loopRegions) {
        if (region.startIndex == blockIndex) {
            return &region;
        }
    }
    return nullptr;
}

void Workflow::setLoopRegions(std::vector<WorkflowLoopRegion> regions) {
    m_loopRegions = std::move(regions);
    normalizeLoopRegions();
}

void Workflow::normalizeLoopRegions() {
    const int blockCount = static_cast<int>(m_blocks.size());
    std::vector<WorkflowLoopRegion> valid;
    valid.reserve(m_loopRegions.size());
    for (WorkflowLoopRegion region : m_loopRegions) {
        if (blockCount <= 0) {
            continue;
        }
        region.startIndex = std::clamp(region.startIndex, 0, blockCount - 1);
        region.endIndex = std::clamp(region.endIndex, region.startIndex, blockCount - 1);
        if (region.id.empty()) {
            region.id = newLoopRegionId();
        }
        bool overlaps = false;
        for (const WorkflowLoopRegion& existing : valid) {
            if (regionsOverlap(region, existing)) {
                overlaps = true;
                break;
            }
        }
        if (!overlaps) {
            valid.push_back(std::move(region));
        }
    }
    std::sort(valid.begin(), valid.end(),
              [](const WorkflowLoopRegion& a, const WorkflowLoopRegion& b) {
                  return a.startIndex < b.startIndex;
              });
    m_loopRegions = std::move(valid);
}

void Workflow::addBlock(std::unique_ptr<Block> block) {
    m_blocks.push_back(std::move(block));
}

void Workflow::insertBlock(int index, std::unique_ptr<Block> block) {
    if (index < 0 || index > static_cast<int>(m_blocks.size())) {
        index = static_cast<int>(m_blocks.size());
    }
    m_blocks.insert(m_blocks.begin() + index, std::move(block));
    adjustLoopRegionsAfterBlockInserted(index);
}

void Workflow::replaceBlock(int index, std::unique_ptr<Block> block) {
    if (index < 0 || index >= static_cast<int>(m_blocks.size()) || !block) {
        return;
    }
    m_blocks[index] = std::move(block);
}

void Workflow::removeBlock(int index) {
    if (index < 0 || index >= static_cast<int>(m_blocks.size())) {
        return;
    }
    m_blocks.erase(m_blocks.begin() + index);
    adjustLoopRegionsAfterBlockRemoved(index);
}

void Workflow::moveBlockUp(int index) {
    if (index <= 0 || index >= static_cast<int>(m_blocks.size())) {
        return;
    }
    moveBlock(index, index - 1);
}

void Workflow::moveBlockDown(int index) {
    if (index < 0 || index + 1 >= static_cast<int>(m_blocks.size())) {
        return;
    }
    moveBlock(index, index + 1);
}

void Workflow::moveBlock(int fromIndex, int toIndex) {
    const int size = static_cast<int>(m_blocks.size());
    if (fromIndex < 0 || fromIndex >= size || toIndex < 0 || toIndex >= size || fromIndex == toIndex) {
        return;
    }

    remapLoopRegionIndices([fromIndex, toIndex](int oldIndex) {
        if (oldIndex == fromIndex) {
            return toIndex;
        }
        if (fromIndex < toIndex) {
            if (oldIndex > fromIndex && oldIndex <= toIndex) {
                return oldIndex - 1;
            }
        } else if (oldIndex >= toIndex && oldIndex < fromIndex) {
            return oldIndex + 1;
        }
        return oldIndex;
    });

    if (fromIndex < toIndex) {
        std::rotate(m_blocks.begin() + fromIndex, m_blocks.begin() + fromIndex + 1,
                    m_blocks.begin() + toIndex + 1);
    } else {
        std::rotate(m_blocks.begin() + toIndex, m_blocks.begin() + fromIndex,
                    m_blocks.begin() + fromIndex + 1);
    }
}

void Workflow::clear() {
    m_blocks.clear();
    m_loopRegions.clear();
}

void Workflow::assignFrom(const Workflow& other) {
    clear();
    for (const auto& block : other.blocks()) {
        addBlock(block->clone());
    }
    m_loopRegions = other.loopRegions();
    normalizeLoopRegions();
}

std::unique_ptr<Workflow> Workflow::clone() const {
    auto copy = std::make_unique<Workflow>();
    for (const auto& block : m_blocks) {
        copy->addBlock(block->clone());
    }
    copy->m_loopRegions = m_loopRegions;
    copy->normalizeLoopRegions();
    return copy;
}

void Workflow::adjustLoopRegionsAfterBlockRemoved(int removedIndex) {
    std::vector<WorkflowLoopRegion> updated;
    updated.reserve(m_loopRegions.size());
    for (WorkflowLoopRegion region : m_loopRegions) {
        if (removedIndex < region.startIndex) {
            region.startIndex--;
            region.endIndex--;
            updated.push_back(region);
        } else if (removedIndex > region.endIndex) {
            updated.push_back(region);
        } else {
            region.endIndex--;
            if (region.startIndex > region.endIndex) {
                continue;
            }
            updated.push_back(region);
        }
    }
    m_loopRegions = std::move(updated);
    normalizeLoopRegions();
}

void Workflow::adjustLoopRegionsAfterBlockInserted(int insertIndex) {
    for (WorkflowLoopRegion& region : m_loopRegions) {
        if (insertIndex <= region.startIndex) {
            region.startIndex++;
            region.endIndex++;
        } else if (insertIndex <= region.endIndex + 1) {
            region.endIndex++;
        }
    }
    normalizeLoopRegions();
}

void Workflow::remapLoopRegionIndices(const std::function<int(int oldIndex)>& mapIndex) {
    for (WorkflowLoopRegion& region : m_loopRegions) {
        region.startIndex = mapIndex(region.startIndex);
        region.endIndex = mapIndex(region.endIndex);
        if (region.startIndex > region.endIndex) {
            std::swap(region.startIndex, region.endIndex);
        }
    }
    normalizeLoopRegions();
}
