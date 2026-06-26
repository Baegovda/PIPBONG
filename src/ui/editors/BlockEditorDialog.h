#pragma once

#include "core/workflow/Block.h"
#include "core/workflow/blocks/ClickBlock.h"
#include "core/workflow/blocks/CommentBlock.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/LoopBlock.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include "core/workflow/blocks/WaitBlock.h"

#include "app/FeatureHotkeyGate.h"

#include <QDialog>
#include <memory>

class QButtonGroup;
class QLabel;
class QCloseEvent;
class QShowEvent;
class QStackedWidget;
class ClickEditor;
class CommentEditor;
class LoopEditor;
class ImageFindEditor;
class KeyPressEditor;
class WaitEditor;

class BlockEditorDialog : public QDialog {
    Q_OBJECT
public:
    BlockEditorDialog(Block* block, const QString& projectDirectory, QWidget* parent = nullptr);

    std::unique_ptr<Block> takeBlock();

private:
    void setupUi();
    void setupTypeButtons(class QHBoxLayout* row);
    void syncTypeButtonSelection(BlockType type);
    int stackIndexForType(BlockType type) const;
    void reloadEditorForType(BlockType type);
    void onTypeSelected(BlockType newType);
    void fitToCurrentPage();
    void dismissCaptureOverlays();

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

    BlockType m_initialType = BlockType::ImageFind;
    FeatureHotkeyGateScope m_featureHotkeyGate;
    std::unique_ptr<Block> m_block;
    QString m_projectDirectory;

    QButtonGroup* m_typeButtonGroup = nullptr;
    QLabel* m_typeChangeNote = nullptr;
    QStackedWidget* m_stack = nullptr;

    ImageFindEditor* m_imageFindEditor = nullptr;
    ClickEditor* m_clickEditor = nullptr;
    KeyPressEditor* m_keyPressEditor = nullptr;
    WaitEditor* m_waitEditor = nullptr;
    LoopEditor* m_loopEditor = nullptr;
    CommentEditor* m_commentEditor = nullptr;

    std::unique_ptr<Block> m_imageFindFormBlock;
    std::unique_ptr<Block> m_clickFormBlock;
    std::unique_ptr<Block> m_keyPressFormBlock;
    std::unique_ptr<Block> m_waitFormBlock;
    std::unique_ptr<Block> m_loopFormBlock;
    std::unique_ptr<Block> m_commentFormBlock;
};
