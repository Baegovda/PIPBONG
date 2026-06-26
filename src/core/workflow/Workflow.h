#pragma once

#include "core/workflow/Block.h"
#include "core/workflow/WorkflowLoopRegion.h"

#include <functional>
#include <memory>
#include <vector>

class Workflow {
public:
    Workflow() = default;

    const std::vector<std::unique_ptr<Block>>& blocks() const { return m_blocks; }
    std::vector<std::unique_ptr<Block>>& blocks() { return m_blocks; }

    const std::vector<WorkflowLoopRegion>& loopRegions() const { return m_loopRegions; }
    std::vector<WorkflowLoopRegion>& loopRegions() { return m_loopRegions; }

    const WorkflowLoopRegion* loopRegionStartingAt(int blockIndex) const;
    void setLoopRegions(std::vector<WorkflowLoopRegion> regions);
    void normalizeLoopRegions();

    void addBlock(std::unique_ptr<Block> block);
    void insertBlock(int index, std::unique_ptr<Block> block);
    void replaceBlock(int index, std::unique_ptr<Block> block);
    void removeBlock(int index);
    void moveBlockUp(int index);
    void moveBlockDown(int index);
    void moveBlock(int fromIndex, int toIndex);
    void clear();
    void assignFrom(const Workflow& other);

    std::unique_ptr<Workflow> clone() const;

private:
    void adjustLoopRegionsAfterBlockRemoved(int removedIndex);
    void adjustLoopRegionsAfterBlockInserted(int insertIndex);
    void remapLoopRegionIndices(const std::function<int(int oldIndex)>& mapIndex);

    std::vector<std::unique_ptr<Block>> m_blocks;
    std::vector<WorkflowLoopRegion> m_loopRegions;
};
