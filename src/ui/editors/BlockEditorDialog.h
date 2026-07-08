#pragma once

#include "core/workflow/Block.h"

#include "core/workflow/blocks/ClickBlock.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "core/workflow/blocks/TextBlock.h"

#include "app/FeatureHotkeyGate.h"

#include <QDialog>

#include <memory>

class QButtonGroup;
class QLabel;
class QCloseEvent;
class QShowEvent;
class QStackedWidget;
class ClickEditor;
class ImageFindEditor;
class KeyPressEditor;
class WaitEditor;
class TextEditor;

class BlockEditorDialog : public QDialog {
    Q_OBJECT
public:
    BlockEditorDialog(Block* block, const QString& projectDirectory, QWidget* parent = nullptr);

    void setWorkflowEditorContext(int blockCount, int editingBlockIndex);
    void setRoiCorrectionUiPolicy(bool featureGlobalEnabled, bool sessionEligible);
    void setClickFeatureRunOptions(bool lockMouseToScreenCenterDuringRun,
                                   bool lockMouseToCurrentPositionDuringRun);
    bool lockMouseToScreenCenterDuringRun() const;
    bool lockMouseToCurrentPositionDuringRun() const;
    std::unique_ptr<Block> takeBlock();

private:
    void setupUi();
    void setupTypeButtons(class QHBoxLayout* row);
    void syncTypeButtonSelection(BlockType type);
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
    int m_workflowBlockCount = 0;
    int m_editingBlockIndex = -1;

    QButtonGroup* m_typeButtonGroup = nullptr;
    QLabel* m_typeChangeNote = nullptr;
    QStackedWidget* m_stack = nullptr;

    ImageFindEditor* m_imageFindEditor = nullptr;
    ClickEditor* m_clickEditor = nullptr;
    KeyPressEditor* m_keyPressEditor = nullptr;
    WaitEditor* m_waitEditor = nullptr;
    TextEditor* m_textEditor = nullptr;

    std::unique_ptr<Block> m_imageFindFormBlock;
    std::unique_ptr<Block> m_clickFormBlock;
    std::unique_ptr<Block> m_keyPressFormBlock;
    std::unique_ptr<Block> m_waitFormBlock;
    std::unique_ptr<Block> m_textFormBlock;
};
